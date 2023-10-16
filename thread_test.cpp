#include <iostream>
#include "threadpool.h"
#include <memory>
using namespace std;

int count=0;
mutex mut;

void task(){

    for(int i=1;i<=10;i++){
        this_thread::sleep_for(chrono::milliseconds(1000));
        unique_lock<mutex> lck(mut);
        count++;
        cout<<" count = "<<count<<endl;
    }

}

void do_other_things(){
    this_thread::sleep_for(chrono::seconds(10));
    cout<<"do other things!"<<endl;
}

int main(){
    threadpool mypool(8,100);
    cout<<mypool.threadpool_GetAliveSize()<<endl;
    mypool.threadpool_Start();
    cout<<mypool.threadpool_GetAliveSize()<<endl;

    for(int i=0;i<20;i++)
    mypool.threadpool_AddTask(&task);

    mypool.threadpool_WaitEnd(-1);

    cout<<"here"<<endl;
    mypool.threadpool_AddTask(&do_other_things);

    mypool.threadpool_WaitEnd(-1);
    mypool.threadpool_Stop();
    return 0;
}