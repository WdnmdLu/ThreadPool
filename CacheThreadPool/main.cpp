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
            for(int j = 0;j< 1000; j++){}
        }
        std::cout<<"Finish Work"<<std::endl;
        return sum;
    }
    int begin,end;
};

int main(){    
    ThreadPool Pool;
    //Pool.SetMode(PoolMode::MODE_CACHED);
    Pool.SetTaskMax(4);
    Pool.Start();
        //创建一个任务
    std::shared_ptr<MyTask> task1 = std::make_shared<MyTask>(1,10000);
    std::shared_ptr<MyTask> task2 = std::make_shared<MyTask>(10001,20000);
    std::shared_ptr<MyTask> task3 = std::make_shared<MyTask>(20001,30000);
    std::shared_ptr<MyTask> task4 = std::make_shared<MyTask>(30001,40000);
    std::shared_ptr<MyTask> task5 = std::make_shared<MyTask>(40001,50000);
    

    Result res1 = Pool.SubmitTask(task1);
    Result res2 = Pool.SubmitTask(task2);
    Result res3 = Pool.SubmitTask(task3);
    Result res4 = Pool.SubmitTask(task4);
    for(int i=0;i<10;i++)
    {
        Pool.SubmitTask(task5);
    }
    // 等待线程池完成任务执行
    sleep(1);
    Pool.~ThreadPool();
    sleep(1);
    int num = res1.Get().cast_<int>();
    num += res2.Get().cast_<int>();
    num += res3.Get().cast_<int>();
    num += res4.Get().cast_<int>();
    
    cout<<"Num: "<< num <<endl;

    cout<<"OK!"<<endl;
    exit(1);
}
