#ifndef JOBS_H
#define JOBS_H

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>

inline std::queue<std::function<void()>> jobs;
inline std::condition_variable threadNotifier;
inline std::mutex jobSyncMutex;

void InitJobPool(long numThreads);
void ShutdownJobPool();

// TODO: When C++23 is ready, use move_only_function and move the promise directly

template <typename Callable, typename... Args, typename T = std::invoke_result_t<std::decay_t<Callable>, std::decay_t<Args>...>>
void EmplaceJob(Callable job, std::shared_ptr<std::promise<T>>& promise, Args&&... args) {
    jobs.emplace([job, promise, args...]() {
        T returned = job(args...);
        promise->set_value(returned);
    });
}

template <typename Callable, typename... Args>
inline void EmplaceJob(Callable job, std::shared_ptr<std::promise<void>>& promise, Args&&... args) {
    jobs.emplace([job, promise, args...]() {
        job(args...);
        promise->set_value();
    });
}

template <typename Callable, typename... Args, typename T = std::invoke_result_t<std::decay_t<Callable>, std::decay_t<Args>...>>
std::future<T> StartJob(Callable&& job, Args&&... args) {

    auto promise = std::make_shared<std::promise<T>>();
    auto future = promise->get_future();

    {
        std::lock_guard g(jobSyncMutex);
        EmplaceJob(job, promise, args...);
        threadNotifier.notify_one();
    }

    return future;

}

#endif