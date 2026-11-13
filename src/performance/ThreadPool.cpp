#include "performance/ThreadPool.hpp"

namespace workbench::performance {

ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) numThreads = 1;
    m_threads.reserve(numThreads);

    for (size_t i = 0; i < numThreads; ++i) {
        m_threads.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->m_queueMutex);
                    this->m_cvTask.wait(lock, [this]() {
                        return this->m_stop.load() || !this->m_tasks.empty();
                    });

                    if (this->m_stop.load() && this->m_tasks.empty()) {
                        return;
                    }

                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                    this->m_activeTasks++;
                }

                task();

                {
                    std::unique_lock<std::mutex> lock(this->m_queueMutex);
                    this->m_activeTasks--;
                    if (this->m_tasks.empty() && this->m_activeTasks == 0) {
                        this->m_cvFinished.notify_all();
                    }
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    m_stop = true;
    m_cvTask.notify_all();
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_tasks.push(std::move(task));
    }
    m_cvTask.notify_one();
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_cvFinished.wait(lock, [this]() {
        return this->m_tasks.empty() && this->m_activeTasks == 0;
    });
}

} // namespace workbench::performance

