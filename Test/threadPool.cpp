#include "threadPool.h"
#include <functional>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <thread>
#include <sys/time.h>

//////////////////////////////////////////////////////
//判断线程池是否处于运行状态
bool ThreadPool::checkIsRunning() const{
    return this->isRunning;
}

//线程池构造函数
ThreadPool::ThreadPool() : 
    InitThreadSize(ThreadMin),
    curThreadSize(ThreadMin),
    MaxThreadSize(8),
    IdleThreadSize(ThreadMin),
    TaskSize(0),
    TaskSizeMax(1024),
    Mode(MODE_FIXED),
    isRunning(false) {
    // 互斥锁和条件变量初始化
    pthread_mutex_init(&QueueMut, NULL);
    pthread_cond_init(&TaskNotFull, NULL);
    pthread_cond_init(&TaskNotEmpty, NULL);
    pthread_cond_init(&exitPool, NULL);
}
//设置线程池的线程数量上限阈值
void ThreadPool::SetThreadMax(int Size){
    if(checkIsRunning()){
        std::cout<<"线程池已经启动无法设置阈值"<<std::endl;
        return;
    }
    if(this->Mode == MODE_FIXED){
        std::cout<<"线程池模式为Fixed模式,线程数量是固定的无法修改"<<std::endl;
        this->MaxThreadSize = ThreadMax;
        return;
    }
    this->MaxThreadSize = Size;
}
//线程池退出，任务一定全部已经提交完成，任务不一定全部都被执行完
ThreadPool::~ThreadPool() {
    //将线程池的状态由运行设置为停止模式
    isRunning = false;
    //线程回收
    //将阻塞的线程全部回收
    pthread_mutex_lock(&QueueMut);
    //通知所有阻塞在这个信号量上的线程开始退出
    pthread_cond_signal(&TaskNotEmpty);
    while(Thread_List.size()!=0){
        pthread_cond_wait(&exitPool,&QueueMut);
    }
    std::cout<<"线程池成功退出"<<std::endl;
    return;
    //等待运行的线程结束运行，然后回收
}
//设置线程池的模式
void ThreadPool::SetMode(PoolMode Mode) {
    if(this->isRunning){
        return;
    }
    this->Mode = Mode;
}
//设置任务的存放上限
void ThreadPool::SetTaskMax(int Size) {
    if(this->isRunning){
        return;
    }
    this->TaskSizeMax = Size;
}
//开始线程池的运行
void ThreadPool::Start(size_t Size) {
    isRunning = true;
    InitThreadSize = Size;
    // 创建所有的线程对象
    for (size_t i = 0; i < InitThreadSize; i++) {
        //使用unqie_ptr创建一个对象
        std::unique_ptr<Thread> ptr(new Thread(std::bind(&ThreadPool::ThreadHandler, this, std::placeholders::_1)));
        //要传入这个对象的时候要使用move       
        Thread_List.emplace(ptr->getId(), std::move(ptr));
        //std::cout<<"线程"<<ptr->getId()<<"创建"<<std::endl;
    }
    // 启动线程
    for (const auto& pair : Thread_List) {
        pair.second->Start();
    }
    IdleThreadSize = Size;
    curThreadSize = Size;
}
int temp = 1;
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
    pthread_cond_signal(&TaskNotEmpty);
    //失败在TaskNotFull通知

    //cache模式，任务处理比较紧急，场景：小而快的任务，耗时的任务还是使用Fixed模式
    std::cout<<"任务数量"<<TaskSize<<" 空闲线程数量"<<IdleThreadSize<<std::endl;
    std::cout<<"当前线程数量 "<<curThreadSize<<" 最大线程数量"<<MaxThreadSize<<std::endl;
    if(Mode == PoolMode::MODE_CACHED && TaskSize > IdleThreadSize && curThreadSize < MaxThreadSize){
        std::cout<<"创建"<<temp<<"个新线程"<<std::endl;
        temp++;
        // 创建一个新线程
        std::unique_ptr<Thread> ptr(new Thread(std::bind(&ThreadPool::ThreadHandler, this, std::placeholders::_1)));
        uint16_t id = ptr->getId();
        // 把线程放入 unordered_map
        Thread_List.emplace(id, std::move(ptr));
        // 启动这个线程
        Thread_List[id]->Start();
        curThreadSize++;
        IdleThreadSize++;
    }
    return Result(Data);
}


///////////////////////////////////////////////////////////////////////////
//Result结果获取实现

Any Result::Get(){
    if(!isValid){
        return "";
    }
    sem.wait();
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

uint16_t Thread::Id = 1;

void ThreadPool::ThreadHandler(uint16_t id){
    std::cout<<curThreadSize<<std::endl;
    while(1){
        //获取锁
        pthread_mutex_lock(&QueueMut);

        //任务队列不空，取任务
        //任务队列为空，解锁等待
        //线程池模式为Cache模式,且当前线程数量大于下限
        if(Mode == PoolMode::MODE_CACHED && curThreadSize > ThreadMin){
            struct timespec timeout;
            int ret;
            while(TaskQueue.size() == 0){

                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 3;
                ret = pthread_cond_timedwait(&TaskNotEmpty, &QueueMut, &timeout);
                //线程超时退出,只要发生超时时间一个经过了1s,因此需要将该线程删除
                if(!checkIsRunning()){
                    break;
                }
                if (ret == ETIMEDOUT) {
                    if(curThreadSize > ThreadMin){
                        //当前线程数量大于上限值
                        std::cout<<"线程"<<id<<"超时"<<std::endl;   

                        curThreadSize--;
                        IdleThreadSize--;

                        this->Thread_List.erase(id);
                        std::cout<<Thread_List.size()<<std::endl;
                        //     //将运行的这个函数直接退出
                        std::cout<<"当前线程数量"<<curThreadSize<<" "<<"空闲线程数量"<<IdleThreadSize<<" "<< 
                                    "线程数量下限值"<<ThreadMin<<std::endl;
                        pthread_mutex_unlock(&QueueMut);     
                        return;
                    }
                    continue;
                }
                else{
                    std::cout<<"线程"<<id<<"获取任务"<<std::endl;
                    break;
                }
            }
            
        }
        //线程池模式为Fixed模式
        else{
            while(TaskQueue.size() == 0){
                std::cout<<"线程"<<id<<"进入阻塞"<<std::endl;
                pthread_cond_wait(&TaskNotEmpty, &QueueMut);
                if(!checkIsRunning()){
                    break;
                }
            }
        }
        //任务全部结束才真正退出
        if(!checkIsRunning() && TaskQueue.size() == 0){
            std::cout<<"线程"<<id<<"开始退出"<<std::endl;
            curThreadSize--;
            IdleThreadSize--;
            //线程删除，退出
            this->Thread_List.erase(id);
            pthread_mutex_unlock(&QueueMut);
            //如果是最后一个线程则直接通知析构函数然后直接退出
            if(Thread_List.size() == 0){
                pthread_cond_signal(&exitPool);
                return;
            }
            //否则接着通知其它的线程退出
            pthread_cond_signal(&TaskNotEmpty);
            return;
        }
        std::cout<<"线程"<<id<<"执行任务"<<TaskSize<<std::endl;
        //线程取到了任务，空闲线程--
        IdleThreadSize--;
        //取任务
        std::shared_ptr<Task> sp = TaskQueue.front();
        TaskQueue.pop();
        //通知生产者用户放入任务
        TaskSize--;//任务数量--
        pthread_mutex_unlock(&QueueMut);
        //消费者线程消费了一个任务，发出队列不满信号，通知生产者用户放入任务
        pthread_cond_signal(&TaskNotFull);
        //执行任务
        sp->exec();
        //执行完任务，空闲线程数量++
        IdleThreadSize++;
        std::cout<<"线程"<<id<<"完成任务"<<std::endl;
        //lastTime = std::chrono::high_resolution_clock::now(); // 任务执行完成更新时间
    }
    return;
}
//线程处理任务
//开启线程
void Thread::Start() {
    std::thread t(this->func,ThreadId);
    t.detach();
}

uint16_t Thread::getId() const{
    return ThreadId;
}

//线程构造函数
Thread::Thread(Thread::ThreadFunc Data) : func(Data) {
    this->ThreadId=Id;
    Id++;
}
//线程的析构函数
Thread::~Thread(){}

/////////////////////////////////////////////////
//Task任务相关函数实现
void Task::setResult(Result *Res)
{
    this->P=Res;
}

void Task::exec(){
    this->P->setValue(this->Run());
}