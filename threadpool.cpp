#include "threadpool.h"

using namespace std;


bool threadpool::threadpool_InitSize(size_t min,size_t max)
{
    std::unique_lock<std::mutex> lck(_mutex);   //拿到锁

    if(!_threads.empty()){
        std::cout<<"error: 线程池已在工作！"<<std::endl;
        return false;
    }
    
    _min_thread_num=min;
    _max_thread_num=max;
    return true;
}

int threadpool::threadpool_Start()
{
    std::unique_lock<std::mutex>lck(_mutex);
    if(!_threads.empty()){
        std::cout<<"error: 线程池已在工作！"<<std::endl;
        return 1;
    }

    _manager=new std::thread(&threadpool::manager_callback,this);
    std::cout<<"Manager thread "<<_manager->get_id()<<" is created!"<<std::endl;

    for(int i=0;i<_min_thread_num;i++){
        _threads.push_back(new std::thread(&threadpool::thread_callback,this));
        std::cout<<"(Init)thread "<<_threads[i]->get_id()<<" is created!"<<std::endl;
        _alive_thread_num++;
    }

    return 0;
}

bool threadpool::threadpool_WaitEnd(int extime)
{
    std::unique_lock<std::mutex> lck(_mutex);
    if(_Task_queue.empty() && _atomic == 0)return true;

    if(extime <= 0) _cond.wait(lck,[this]{return _Task_queue.empty() && _atomic == 0;});
    else _cond.wait_for(lck,std::chrono::milliseconds(extime),[this]{return _Task_queue.empty() && _atomic == 0;});

    return true;
}

void threadpool::threadpool_Stop()
{
    {
        std::unique_lock<std::mutex> lck(_mutex);

        _status = QUIT;

        _cond.notify_all();
    }
    
    //此处等待所有线程返回 必须释放锁
    if(_manager->joinable())_manager->join();
    delete _manager;
    _manager=nullptr;

    for(int i=0;i<_threads.size();i++){
        if(_threads[i]->joinable())_threads[i]->join();
        delete _threads[i];
        _threads[i]=nullptr;
    }

    

    std::unique_lock<std::mutex> lck(_mutex);
    _threads.clear();
}

bool threadpool::gettask(TaskFunc_ptr &task)
{
    std::unique_lock<std::mutex> lck(_mutex);
    //如果任务队列为空 则线程阻塞等待
    if(_Task_queue.empty()){
        _cond.wait(lck,[this]{
                return this->_status == QUIT || !this->_Task_queue.empty() || this->_destroy_thread_num != 0;
            });
    }
    
    //如果管理者判断要销毁线程
    if(_destroy_thread_num !=0){
        _destroy_thread_num -- ;
        _alive_thread_num -- ;
        return false;
    }

    //如果要退出了  就不允许继续拿取任务 返回false
    if(this->_status == QUIT)return false;

    //否则 一切正常 拿取任务
    if(!_Task_queue.empty()){
        task=std::move(_Task_queue.front());    //避免拷贝
        _Task_queue.pop();
        return true;
    }
    return true;
}

void threadpool::thread_callback()
{
    while(this->_status == EXSIT){
        TaskFunc_ptr task;
        int ret=gettask(task);
        if(ret){
            //如果正常抓取到了任务
            std::unique_lock<std::mutex> lck(_mutex);
            _busy_thread_num++;
            lck.unlock();

            ++_atomic;

            try
            {
                if(task->_extime > 0 && task->_extime < TNOWMS){
                    //任务超时
                }
                else {
                    task->_func();
                }
            }
            catch(...)
            {
                std::cout<<"任务执行出错！"<<std::endl;
            }
            
            --_atomic;

            lck.lock();
            _busy_thread_num--;
            lck.unlock();

            lck.lock();
            if(_atomic == 0 && _Task_queue.empty()){
                //如果任务队列为空 且 所有任务都执行完成 则给wait函数广播发送信号
                _cond.notify_all();
            }
        } else {
            std::unique_lock<std::mutex> lck(_mutex);
            std::cout<<"Thread "<<std::this_thread::get_id()<<" is Killed!"<<std::endl;
            return;
        }
    }
    std::cout<<"Thread "<<std::this_thread::get_id()<<" is Exited!"<<std::endl;
    return ;
}

void threadpool::manager_callback()
{
    //每隔2秒检测一次
    while(this->_status != QUIT){

        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::unique_lock<std::mutex> lck(_mutex);

        //增加线程策略
        if(_Task_queue.size() >= MAXTASKNUM && _alive_thread_num<_max_thread_num){
            for(int i=0;i<=0 && _alive_thread_num < _max_thread_num;i++){
                _threads.push_back(new std::thread(thread_callback,this));
                _alive_thread_num++;
                std::cout<<"thread "<<_threads[_alive_thread_num-1]->get_id()<<"is created!"<<std::endl;
                
            }
        }

        //删除线程策略
        if(_alive_thread_num > _min_thread_num && 2*_busy_thread_num <_alive_thread_num){
            _destroy_thread_num ++;
            _cond.notify_all();
        }
    }
}

int gettimeofday(struct timeval &tv)
{
#if WIN32
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year   = wtm.wYear - 1900;
    tm.tm_mon   = wtm.wMonth - 1;
    tm.tm_mday   = wtm.wDay;
    tm.tm_hour   = wtm.wHour;
    tm.tm_min   = wtm.wMinute;
    tm.tm_sec   = wtm.wSecond;
    tm. tm_isdst  = -1;
    clock = mktime(&tm);
    tv.tv_sec = clock;
    tv.tv_usec = wtm.wMilliseconds * 1000;

    return 0;
#else
    return ::gettimeofday(&tv, 0);
#endif
}

void getNow(timeval *tv)
{
#if TARGET_PLATFORM_IOS || TARGET_PLATFORM_LINUX

    int idx = _buf_idx;
    *tv = _t[idx];
    if(fabs(_cpu_cycle - 0) < 0.0001 && _use_tsc)
    {
        addTimeOffset(*tv, idx);
    }
    else
    {
        TC_Common::gettimeofday(*tv);
    }
#else
    gettimeofday(*tv);
#endif
}

int64_t getNowMs()
{
    struct timeval tv;
    getNow(&tv);

    return tv.tv_sec * (int64_t)1000 + tv.tv_usec / 1000;
}

