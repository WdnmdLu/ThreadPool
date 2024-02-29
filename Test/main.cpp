#include "threadPool.h"
#include <unistd.h>
#include <time.h>
#include <thread>
#include <chrono>
#include <iostream>
using namespace std;
class MyTask:public Task{
public:
    MyTask(int Begin,int End):begin(Begin),end(End){}
    Any Run(){
        int i,sum=0;
        for(i=begin;i<=end;i++){
            sum+=i;
        }
        return sum;
    }
    int begin,end;
};

int main(){    
    ThreadPool Pool;
    //Pool.SetMode(PoolMode::MODE_CACHED);
    Pool.SetTaskMax(4);
    Pool.Start(std::thread::hardware_concurrency());
    sleep(1);
        //创建一个任务
        // std::shared_ptr<MyTask> task1 = std::make_shared<MyTask>(1,10000);
        // std::shared_ptr<MyTask> task2 = std::make_shared<MyTask>(10001,20000);
        // std::shared_ptr<MyTask> task3 = std::make_shared<MyTask>(20001,30000);
        // std::shared_ptr<MyTask> task4 = std::make_shared<MyTask>(30001,40000);

    // Pool.~ThreadPool();
    // cout<<"Hello"<<endl;

    // cout<<"OK!"<<endl;
}
