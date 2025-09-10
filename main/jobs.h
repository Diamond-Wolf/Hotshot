#ifndef JOBS_H
#define JOBS_H

#include <functional>
#include <future>
#include <mutex>
#include <queue>

inline std::queue<std::function<void()>> jobs;
inline std::condition_variable threadNotifier;
inline std::mutex jobQueueMutex;

void InitJobPool(long numThreads);
void ShutdownJobPool();

template <typename T>
std::promise<T> StartJob(const std::function<T()> job) {

    std::promise<T> promise;

    jobs.emplace([job, &promise]() {
        T returned = job();
        promise.set_value(returned);
    });

    threadNotifier.notify_one();

    return promise;

}

template<>
std::promise<void> StartJob(const std::function<void()> job) {

    std::lock_guard g(jobQueueMutex);

    std::promise<void> promise;

    jobs.emplace([job, &promise]() {
        job();
        promise.set_value();
    });

    threadNotifier.notify_one();

    return promise;

}

#endif