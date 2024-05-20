/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;
#include "Semaphore.h"
#include <iostream>

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads), tasksSemaphore(0), workersSemaphore(0), workers(numThreads)
{
    activeWorkers = 0;
    stop = false;

    dt = thread([this]{dispatcher();}); // initialize the dispatcher thread

    for(size_t i = 0; i < numThreads; i++)
    {
        wts[i] = thread([this, i]{worker(i);}); // initialize the worker threads
        workersSemaphore.signal(); // signal the worker threads
    }
}

void ThreadPool::dispatcher()
{
    while (true) {
        tasksSemaphore.wait(); // wait for a task to be available
        if (stop && tasks.empty()) { 
            break;
        }

        function<void(void)> task; 
        {
            unique_lock<std::mutex> lock(queueMutex);
            if (!tasks.empty()) {
                task = move(tasks.front()); 
                tasks.pop();
            }
        }

        if (task) { 
            workersSemaphore.wait(); // wait for a worker to be available
            for (size_t i = 0; i < wts.size(); i++) {
                // unique_lock<std::mutex> lock(queueMutex);
                unique_lock<std::mutex> lock(workers[i].mutex);
                if (!workers[i].busy) {
                    workers[i].task = move(task); // move the task to the worker
                    workers[i].busy = true; // set the worker to busy
                    workers[i].cv.notify_one(); // notify the worker
                    activeWorkers++;
                    break;
                }
            }
        }
    }
}

void ThreadPool::worker(size_t id)
{
    while (true) {
        function<void(void)> task;
        {
            // unique_lock<std::mutex> lock(queueMutex);
            unique_lock<std::mutex> lock(workers[id].mutex);
            workers[id].cv.wait(lock, [this, id] { return stop || workers[id].busy; }); // wait for a task to be available
            if (stop && !workers[id].busy) { 
                break;
            }
            task = move(workers[id].task); 
        }

        if (task) {
            task();
            {
                // unique_lock<std::mutex> lock(queueMutex);
                unique_lock<std::mutex> lock(workers[id].mutex);
                workersSemaphore.signal(); 
                workers[id].busy = false; // set the worker to not busy
                {
                    unique_lock<std::mutex> lock(queueMutex);
                    activeWorkers--;
                    if (tasks.empty() && activeWorkers == 0) {
                        allTasksDoneCondition.notify_all(); // notify all threads that all tasks are done
                    }
                }
                // activeWorkers--;
                // if (tasks.empty() && activeWorkers == 0) {
                //     allTasksDoneCondition.notify_all(); // notify all threads that all tasks are done
                // }
            }
        }
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) 
{
    {
        unique_lock<std::mutex> lock(queueMutex); 
        tasks.emplace(thunk); // add the task to the queue
    }
    tasksSemaphore.signal(); // signal the dispatcher thread
}

void ThreadPool::wait()  
{
    unique_lock<std::mutex> lock(queueMutex);
    allTasksDoneCondition.wait(lock, [this] { return tasks.empty() && activeWorkers == 0; }); // wait for all tasks to be done
}

ThreadPool::~ThreadPool() 
{
    {
        unique_lock<std::mutex> lock(queueMutex);
        stop = true; // set the stop flag to true
    }
    tasksSemaphore.signal(); // signal the dispatcher thread to stop
    for (size_t i = 0; i < wts.size(); i++) {
        workers[i].cv.notify_one(); // notify all worker threads to stop
    }
    dt.join(); // join the dispatcher thread
    for (size_t i = 0; i < wts.size(); i++) {
        wts[i].join(); // join all worker threads
    }
}

       