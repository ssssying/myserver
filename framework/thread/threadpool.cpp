//
// Created by DGuco on 17-10-13.
//

#include "threadpool.h"

inline ThreadPool::ThreadPool() :
        ThreadPool(thread::hardware_concurrency()), m_stop(false)
{

}


inline ThreadPool::ThreadPool(size_t threads) :  m_stop(false)
{
    for(size_t i = 0;i<threads;++i)
    {
        thread* th = CreateThread();
        if (th)
        {
            m_mWorkers[th->get_id()] = th;
        }
    }
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_condition.notify_all();
    for(auto it = m_mWorkers.begin();it != m_mWorkers.end();it++)
        it->second->join();
}


inline std::thread* ThreadPool::CreateThread()
{
    std::thread* thread =
            new thread(
                [this]
                {
                    for(;;)
                    {
                        std::function<void()> task;
                        std::unique_lock<std::mutex> lock(this->m_mutex);
                        this->m_condition.wait(lock,
                                              [this]{ return this->m_stop || !this->m_qTasks.empty(); });
                        if(this->m_stop && this->m_qTasks.empty())
                           return;
                        task = std::move(this->m_qTasks.front());
                        this->m_qTasks.pop_front();
                        lock.unlock();
                        task();
                    }
                });
    return thread;
}

bool ThreadPool::IsThisThreadIn() {
    auto it = m_mWorkers.find(this_thread::get_id());
    return it != m_mWorkers.end();
}

bool ThreadPool::IsThisThreadIn(thread* thrd) {
    if (thrd) {
        auto it = m_mWorkers.find(thrd->get_id());
        return it != m_mWorkers.end();
    } else {
        return false;
    }
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::PushTaskBack(F &&f, Args &&args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    if (task == NULL)
    {
        throw std::runtime_error("Create task error ");
    }

    std::future<return_type> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        //当前没有任务
        if (IsThisThreadIn() && m_qTasks.size() <= 0)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            (*task)();
        }else
        {
            m_qTasks.emplace_back([task](){ (*task)(); });
        }
    }

    m_condition.notify_one();
    return res;
}


template<class F, class... Args>
auto ThreadPool::PushTaskFront(F &&f, Args &&args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    if (task == NULL)
    {
        throw std::runtime_error("Create task error ");
    }

    std::future<return_type> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_stop)
            throw std::runtime_error("ThreadPool has stopped");
        if (IsThisThreadIn())
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            (*task)();
        }else
        {
            m_qTasks.emplace_front([task](){ (*task)(); });
        }
    }
    m_condition.notify_one();
    return res;
}