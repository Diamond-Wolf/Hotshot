#ifndef JOBS_H
#define JOBS_H

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>

inline std::queue<std::function<void()>> jobs;
inline std::condition_variable threadNotifier;
inline std::mutex jobQueueMutex;

void InitJobPool(long numThreads);
void ShutdownJobPool();

// TODO: When C++23 is ready, use move_only_function and move the promise directly

template <typename T>
void EmplaceJob(const std::function<T()>& job, std::shared_ptr<std::promise<T>>& promise) {
    jobs.emplace([job, promise]() {
        T returned = job();
        promise->set_value(returned);
    });
}

template <>
inline void EmplaceJob(const std::function<void()>& job, std::shared_ptr<std::promise<void>>& promise) {
    jobs.emplace([job, promise]() {
        job();
        promise->set_value();
    });
}

template <typename T>
std::future<T> StartJob(const std::function<T()> job) {

    auto promise = std::make_shared<std::promise<T>>();
    auto future = promise->get_future();
    
    {
        std::lock_guard g(jobQueueMutex);
        EmplaceJob(job, promise);
        threadNotifier.notify_one();
    }

    return future;

}

#endif