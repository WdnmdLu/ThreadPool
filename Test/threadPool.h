#include <vector>
#include <queue>
#include <atomic>
#include <memory>
#include <functional>
#include <pthread.h>
#include <iostream>
#include <unordered_map>
#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#define ThreadMax 16//线程数量最大上限
#define ThreadMin 4//线程数量最低下限值

class Semaphore
{
public:
    Semaphore(int limit = 0):ResLimit(limit)
    {
        pthread_mutex_init(&Mut,NULL);
        pthread_cond_init(&Cond,NULL);
    }
    ~Semaphore() = default;
    int Get(){
        return ResLimit;
    }
    void wait(){
        pthread_mutex_lock(&Mut);
        if(ResLimit<=0){
            pthread_cond_wait(&Cond,&Mut);
        }
        ResLimit--;
        pthread_mutex_unlock(&Mut);
    }
    void post(){
        pthread_mutex_lock(&Mut);
        ResLimit++;
        pthread_mutex_unlock(&Mut);
        pthread_cond_signal(&Cond);
    }
private:
    int ResLimit;
    pthread_mutex_t Mut;
    pthread_cond_t Cond;
};

class Any{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = default;
	Any& operator=(const Any&) = default;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// 这个构造函数可以让Any类型接收任意其它的数据
	template<typename T>  // T:int    Derive<int>
	Any(T data) : base_(std::make_unique<Derive<T>>(data)){}

	// 这个方法能把Any对象里面存储的data数据提取出来
	template<typename T>
	T cast_(){
		// 我们怎么从base_找到它所指向的Derive对象，从它里面取出data成员变量
		// 基类指针 =》 派生类指针   RTTI,将基类强转成派生类
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr){
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	// 基类类型
	class Base{
	public:
		virtual ~Base() = default;
	};

	// 派生类类型
	template<typename T>
	class Derive : public Base{
	public:
		Derive(T data) : data_(data) {}
		T data_;  // 保存了任意的其它类型
	};

private:
	// 定义一个基类的指针,使用基类指针指向派生类对象
	std::unique_ptr<Base> base_;
};
/////////////////////////////////////////////////////////////
class Task;
class Result {
public:
    //获取结果
    Result(std::shared_ptr<Task> task, bool isValid = true);
    Any Get();//获取任务返回值

    void setValue(Any data);

private:
    Semaphore sem;
    Any data;
    std::shared_ptr<Task> MyTask;
    bool isValid;
};

class ThreadPool;
//任务类，用户继承这个任务类，实现任务类中的Run方法，自定义任务的具体实现内容
class Task{
public:
    //一个任务关联一个Result
    void setResult(Result *Res);

    void exec();

    //用户重写Run方法自定义任务的处理
    //将Run的返回值设置为Any类型，使其可以接收任意的返回值
    virtual Any Run() = 0;
private:
    Result *P;
};
//线程池模式选择
enum PoolMode
{
    MODE_FIXED,
    MODE_CACHED
};
//线程类
class Thread {
public:
    using ThreadFunc = std::function<void(uint16_t)>;
    Thread(ThreadFunc Data);
    ~Thread();
    uint16_t getId() const;
    void Start();
private:
    ThreadFunc func;
    static uint16_t Id;
    uint16_t ThreadId;
};

class ThreadPool {
public:
    // 设置线程池的工作模式
    void SetMode(PoolMode Mode);
    // 设置任务上限
    void SetTaskMax(int Size);
    //设置Cache模式下的线程阈值
    void SetThreadMax(int Size);
    // 给线程池提交任务
    Result SubmitTask(std::shared_ptr<Task> Data);
    ThreadPool();
    ~ThreadPool();
    // 开启线程池
    void Start(size_t Size);

    // 禁止用户对线程池进行拷贝构造和赋值构造
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
private:
    //线程处理函数
    void ThreadHandler(uint16_t id);
    //检查线程池运行状态
    bool checkIsRunning() const;

private:
    //使用unordered_map使得一个键值关联一个thread
    std::unordered_map<uint16_t, std::unique_ptr<Thread>> Thread_List;

    // 线程队列容器，存放所有的线程
    //std::vector<std::unique_ptr<Thread>> Thread_List;
    size_t InitThreadSize; // 初始线程数量
    uint16_t curThreadSize; //当前线程数量
    uint16_t MaxThreadSize; //线程上限数量
    uint16_t IdleThreadSize;//空闲线程的数量

    // 任务队列
    std::queue<std::shared_ptr<Task>> TaskQueue;
    uint32_t TaskSize;// 任务数量
    uint32_t TaskSizeMax; // 任务上限数量
    
    // 互斥访问任务队列
    pthread_mutex_t QueueMut;
    //用户关注在任务队列不满，不满则一直放入任务
    pthread_cond_t TaskNotFull; // 任务队列不满
    //线程关注任务队列不空，任务队列不空则一直强任务执行
    pthread_cond_t TaskNotEmpty; // 任务队列不空
    //线程池退出
    pthread_cond_t exitPool;
    PoolMode Mode; // 当前线程池的工作模式

    //判断当前线程是否在运行
    bool isRunning;
};
#endif