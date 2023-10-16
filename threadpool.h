#ifndef THREADPOOL_CPP
#define THREADPOOL_CPP

#include <iostream>
#include <functional>
#include <future>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <vector>
#include <Windows.h>

#define TNOW      getNow()
#define TNOWMS    getNowMs()

/**
 * @brief 用来获取当前时间
 * 
 * @param tv 
 */
void getNow(timeval *tv);
int64_t getNowMs();
int gettimeofday(struct timeval &tv);

typedef enum{
        EXSIT = 0,
        QUIT
    }THREADPOOL_STATUS;

class threadpool{
/*
成员列表
*/
protected:
///@brief 任务队列相关

    //队列内任务结点类型
    struct TaskFunc{
        TaskFunc(uint64_t extime):_extime(extime){}
        int64_t _extime; //任务超时时间，
        std::function<void()> _func;
    };
    typedef std::shared_ptr<TaskFunc> TaskFunc_ptr; 

    //任务队列
    std::queue<TaskFunc_ptr> _Task_queue;

    //任务队列上限
    const size_t MAXTASKNUM = 10; 

///@brief 线程相关

    //线程个数
    size_t _min_thread_num;
    size_t _max_thread_num;
    size_t _busy_thread_num = 0;
    size_t _alive_thread_num = 0;
    size_t _destroy_thread_num = 0;

    //线程组
    std::vector<std::thread*> _threads;

    //管理者线程
    std::thread* _manager = nullptr;

///@brief 条件变量 && 锁

    //线程用 变量 && 锁
    std::mutex _mutex;
    std::condition_variable  _cond;

    //原子变量
    std::atomic<int> _atomic{0};


///@brief 线程池状态变量
    volatile THREADPOOL_STATUS _status; 


/*
PUBLIC API
*/
public:
    /**
     * @brief 构造函数 初始化状态值
     * 
     */
    threadpool(size_t min,size_t max):_min_thread_num(min),_max_thread_num(max),_status(EXSIT){};
    
    /**
     * @brief 析构 stop线程池 
     * 
     */
    virtual ~threadpool(){threadpool_Stop();}

    /**
     * @brief 获取 各个种类线程个数
     * 
     * @return size_t  线程个数
     */
    size_t threadpool_GetAliveSize(){std::unique_lock<std::mutex> lck(_mutex); return this->_alive_thread_num;}

    size_t threadpool_GetDestroySize(){std::unique_lock<std::mutex> lck(_mutex);return this->_destroy_thread_num;}

    size_t threadpool_GetMinSize(){std::unique_lock<std::mutex> lck(_mutex);return  this->_min_thread_num;}

    size_t threadpool_GetMaxSize(){std::unique_lock<std::mutex> lck(_mutex);return  this->_max_thread_num;}

    size_t threadpool_GetBusySize(){std::unique_lock<std::mutex> lck(_mutex);return  this->_busy_thread_num;}
    /**
     * @brief 获取线程池状态
     * 
     * @return THREADPOOL_STATUS 
     */
    THREADPOOL_STATUS threadpool_GetStatus(){return this->_status;}

    /**
     * @brief 初始化线程个数
     * 
     * @param min 设置线程最小数量
     * @param max 设置线程最大数量
     * @return true 成功设置
     * @return false 
     */
    bool threadpool_InitSize(size_t min,size_t max);

    /**
     * @brief 线程池开始运行
     * 
     * @return int 0 --> 正常创建线程并运行  !0 --> 出错
     */
    int threadpool_Start();

    /**
     * @brief AddTask 往线程池加入任务 
     * 
     * @tparam F 函数名
     * @tparam Args 函数的可变参数列表
     * @param func 函数名
     * @param args 函数名的可变参数列表
     * @return std::future<decltype(func(args...))> 一个future 返回值能用于异步操作
     */
    template <class F,class... Args>
    auto threadpool_AddTask(F&& func,Args... args) -> std::future<decltype(func(args...))>{return this->threadpool_AddTask(0,func,args...);}

    template <class F,class... Args>
    auto threadpool_AddTask(uint16_t extime,F&& func,Args... args) -> std::future<decltype(func(args...))>;

    /**
     * @brief 等待所有线程返回
     * 
     * @param extime 等待时间（单位毫秒 ms） 超时直接返回 / 若为非正数则表永远等待
     * @return true 成功返回
     */
    bool threadpool_WaitEnd(int extime = 0);

    /**
     * @brief stop 等待所有线程返回并销毁线程池
     * 
     */
    void threadpool_Stop();


protected:
    /**
     * @brief 从任务列表试图抓取一个任务
     * 
     * @param task 返回抓取的任务
     * @return true 抓取成功
     * @return false 抓取失败！ 线程池退出
     */
    bool gettask(TaskFunc_ptr &task);

    /**
     * @brief 线程工作函数
     * 
     */
    void thread_callback();

    /**
     * @brief 管理者工作函数
     * 
     */
    void manager_callback();
};


template <class F, class... Args>
auto threadpool::threadpool_AddTask(uint16_t extime, F &&func, Args... args) -> std::future<decltype(func(args...))>
{
    int64_t expireTime =  (extime == 0 ? 0 : TNOWMS + extime);  // 获取现在时间
    using RetType = decltype(func(args...));
    auto packaged_tk=std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(func),std::forward<Args>(args)...));

    TaskFunc_ptr tsk =std::make_shared<TaskFunc>(expireTime);
    tsk->_func=[packaged_tk]{
        (*packaged_tk)();
    };

    std::unique_lock<std::mutex> lck(_mutex);
    _Task_queue.push(tsk);
    _cond.notify_all();

    return packaged_tk->get_future();
}

#endif


