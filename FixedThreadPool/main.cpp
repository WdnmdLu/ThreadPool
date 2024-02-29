#include "threadPool.h"
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <mutex>
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

class Cat{
public:
    
    Cat(std::shared_ptr<Task> task, bool isValid = true):task(task),isValid(isValid){}
    Cat(){}
    int age=10;
private:
    Any data;
    Semaphore sem;
    std::shared_ptr<Task> task;
    bool isValid;
};


int main() {
    ThreadPool Pool;
    Pool.Start(4);
    
    std::shared_ptr<MyTask> task1 = std::make_shared<MyTask>(1, 10000);
    std::shared_ptr<MyTask> task2 = std::make_shared<MyTask>(10001, 20000);
    std::shared_ptr<MyTask> task3 = std::make_shared<MyTask>(20001, 30000);
    std::shared_ptr<MyTask> task4 = std::make_shared<MyTask>(30001, 40000);
    
    
    Result res1 = Pool.SubmitTask(task1);
    Result res2 = Pool.SubmitTask(task2);
    Result res3 = Pool.SubmitTask(task3);
    Result res4 = Pool.SubmitTask(task4);
    
    int Sum = res1.Get().cast_<int>() + res2.Get().cast_<int>() + res3.Get().cast_<int>() + res4.Get().cast_<int>();
   
    std::cout << Sum << std::endl;

    int count = 0;

    for (int i = 1; i <= 40000; i++) {
        count += i;
    }
    
    std::cout << count << std::endl;
}
