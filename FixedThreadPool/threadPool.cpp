#include "threadPool.h"
#include <functional>
#include <pthread.h>
#include <time.h>
#include <thread>
//////////////////////////////////////////////////////
//线程池构造函数
ThreadPool::ThreadPool() : InitThreadSize(4), TaskSize(0), TaskSizeMax(1024), Mode(MODE_FIXED),isRunning(false) {
    // 互斥锁和条件变量初始化
    pthread_mutex_init(&QueueMut, NULL);
    pthread_cond_init(&TaskNotFull, NULL);
    pthread_cond_init(&TaskNotEmpty, NULL);
}

ThreadPool::~ThreadPool() {}
//设置线程池的模式
void ThreadPool::SetMode(PoolMode Mode) {
    if(isRunning == false){
        this->Mode = Mode;
    }
    else{
        std::cout<<"线程池已经启动，无法设置线程池的模式"<<std::endl;
    }
    return;
}

void ThreadPool::SetTaskMax(int Size) {
    this->TaskSizeMax = Size;
}

void ThreadPool::Start(size_t Size) {
    isRunning = true;
    InitThreadSize = Size;
    // 创建所有的线程对象
    for (size_t i = 0; i < InitThreadSize; i++) {
        //使用unqie_ptr创建一个对象
        std::unique_ptr<Thread> ptr(new Thread(std::bind(&ThreadPool::ThreadHandler, this)));
        //要传入这个对象的时候要使用move       
        Thread_List.push_back(std::move(ptr));
    }
    // 启动线程
    for (size_t i = 0; i < InitThreadSize; i++) {
        Thread_List[i]->Start();
    }
    IdleThreadSize = Size;
}

//提交任务
Result ThreadPool::SubmitTask(std::shared_ptr<Task> Data) {
    //获取锁
    pthread_mutex_lock(&QueueMut);

    //线程的通信  等待任务队列有空余
    
    //如果任务队列满
   while(TaskQueue.size() == TaskSizeMax){
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        int ret = pthread_cond_timedwait(&TaskNotFull, &QueueMut, &ts);
        //等待超时任务队列还是满无法放入任务
        if(ret == ETIMEDOUT){
            pthread_mutex_unlock(&QueueMut);
            std::cerr<<"Task Queue is Full, subnit task Fail."<<std::endl;

            return Result(Data, false);
            
        }
    }
    //如果任务队列空余，将任务放入到任务队列中
    TaskQueue.push(Data);
    TaskSize++;
    //成功放入任务后在notEmpty通知
    pthread_mutex_unlock(&QueueMut);
    //因为生产并放入了一个任务所以任务队列不为空，通过这个条件变量对消费者线程进行通知让其来消费
    pthread_cond_broadcast(&TaskNotEmpty);
    //失败在TaskNotFull通知

    return Result(Data);
}


///////////////////////////////////////////////////////////////////////////
//Result结果获取实现

Any Result::Get(){
    if(!isValid){
        return "";
    }
    sem.wait();
    std::cout<<"成功获取到信号"<<std::endl;
    return std::move(data);
}

Result::Result(std::shared_ptr<Task> task, bool isValid):MyTask(task),isValid(isValid)
{
    //将当前Res的任务与Res进行了关联
    task->setResult(this);
}

void Result::setValue(Any data){
    this->data=std::move(data);
    this->sem.post();
}

/////////////////////////////////////////////////////////////////////
//线程处理函数，在这里进行线程的处理操作

void* ThreadPool::ThreadHandler(){
    // pthread_t tid = pthread_self();
    while(1){
        //获取锁
        pthread_mutex_lock(&QueueMut);

        //任务队列不空，取任务
        //任务队列为空，解锁等待
        while(TaskQueue.size() == 0){
            
            pthread_cond_wait(&TaskNotEmpty,&QueueMut);
        }
        //线程取到了任务，空闲线程--
        IdleThreadSize--;
        //取任务
        std::shared_ptr<Task> sp = TaskQueue.front();
        TaskQueue.pop();
        //通知生产者用户放入任务
        TaskSize--;
        pthread_mutex_unlock(&QueueMut);
        //消费者线程消费了一个任务，发出队列不满信号，通知生产者用户放入任务
        pthread_cond_broadcast(&TaskNotFull);
        //执行任务
        sp->exec();
        //执行完任务，空闲线程数量++
        IdleThreadSize++;
    }
    return NULL;
}
//线程处理任务
//开启线程
void Thread::Start() {
    std::thread t(this->func);
    t.detach();
}

//线程构造函数
Thread::Thread(Thread::ThreadFunc Data) : func(Data) {}
//线程的析构函数
Thread::~Thread(){

}

/////////////////////////////////////////////////
//Task任务相关函数实现
void Task::setResult(Result *Res)
{
    this->P=Res;
}

void Task::exec(){
    this->P->setValue(this->Run());
}