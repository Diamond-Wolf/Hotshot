#include "jobs.h"

#include <thread>

std::vector<std::thread> threads;
bool quit = false;

void InitJobPool(long numThreads) {
    
    if (numThreads == 0) {

        numThreads = (long)std::thread::hardware_concurrency - 4; //Leave space for non-pooled threads
        if (numThreads < 1)
            numThreads = 1;

    }

    threads.resize(numThreads);
    threads.shrink_to_fit();
    for (int i = 0; i < numThreads; i++) {
        threads[i] = std::thread([]() {

            while (!quit) {

                std::unique_lock l(jobQueueMutex);

                if (jobs.size() == 0) {
                    threadNotifier.wait(l);
                }
                
                auto job = jobs.front();
                jobs.pop();

                l.release();

                job();

            }

        });
    }

}

void ShutdownJobPool() {

    quit = true;

    std::lock_guard g(jobQueueMutex);

    while (jobs.size() > 0)
        jobs.pop();

}