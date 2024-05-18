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

    // initialize the dispatcher thread
    dt = thread([this]{dispatcher();});
    // cout << "Dispatcher thread created" << endl;

    // initialize the worker threads
    for(size_t i = 0; i < numThreads; i++)
    {
        wts[i] = thread([this, i]{worker(i);});
        workersSemaphore.signal(); // signal the worker thread that it is available
        // cout << "Worker " << i << " thread created" << endl;
    }
}

void ThreadPool::dispatcher() // extrae trabajo de la cola, asigna a un worker trabajo especifico y le entrega la funcion a ejecutar a ese trabajador
// {
//     // cout << "Dispatcher waiting" << endl;
//     while (!stop) {
//         tasksSemaphore.wait();
//         // cout << "There is a task" << endl; // si pasa el wait es porque hay una tarea
//         function<void(void)> task;
//         {
//             unique_lock<std::mutex> lock(queueMutex); // lock the queue (adentro de un scope)
//             // if (tasks.empty()) {
//             //     if (stop) break; // si tenemos que terminar y no hay tareas, salimos del loop
//             //     continue;
//             // }
//             // if (!tasks.empty()) {
//             //     task = tasks.front();
//             //     tasks.pop();
//             // }
//             if (stop && tasks.empty()) {
//                 break;
//             }
//             if (!tasks.empty()) {
//                 // cout << "Dispatcher popped task" << endl;
//                 task = move(tasks.front());
//                 tasks.pop();
//             }
//         }
//         if (task) {
//             workersSemaphore.wait();
//             for (size_t i = 0; i < wts.size(); i++) {
//                 unique_lock<std::mutex> lock(queueMutex);
//                 if (workers[i].busy == false) {
//                     workers[i].task = task; // le asigno la tarea al worker - ver si hace falta MOVE
//                     workers[i].busy = true; // el worker esta ocupado
//                     workers[i].cv.notify_one(); // notifico al worker que tiene una tarea
//                     activeWorkers++;
//                     // cout << "Task assigned to worker " << i << endl;
//                     break;
//                 }
//             }
//         }
//     }
//     // cout << "Dispatcher finishing" << endl;
    
// }
{
    while (true) {
        tasksSemaphore.wait();
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
            workersSemaphore.wait();
            for (size_t i = 0; i < wts.size(); i++) {
                unique_lock<std::mutex> lock(queueMutex);
                if (!workers[i].busy) {
                    workers[i].task = move(task); 
                    workers[i].busy = true;
                    workers[i].cv.notify_one();
                    activeWorkers++;
                    break;
                }
            }
        }
    }
}

void ThreadPool::worker(size_t id) // ejecuta la funcion que le asigno el dispatcher (la funcion que se esta encolando)
// {
//     while(!(stop && tasks.empty())) {
//         function<void(void)> task;
//         {
//             unique_lock<std::mutex> lock(queueMutex);
//             workers[id].cv.wait(lock, [this, id] { return stop || workers[id].busy; });
//             if (stop) {
//                 break;
//             }
//             task = move(workers[id].task);
//         }
//         if (task) {
//             // cout << "Worker " << id << " got a task" << endl;
//             workers[id].busy = true;
//             task();
//             {
//                 unique_lock<std::mutex> lock(queueMutex);
//                 workersSemaphore.signal();
//                 workers[id].busy = false;
//                 activeWorkers--;
//                 // cout << "Worker " << id << " finished task" << endl;
//                 // cout << "Active workers: " << activeWorkers << endl;
//                 // cout << "Tasks size: " << tasks.size() << endl;
//                 if (tasks.empty() && activeWorkers == 0) {
//                     allTasksDoneCondition.notify_all();
//                 }

//             }
//         }
//     }
// }
{
    while (true) {
        function<void(void)> task;
        {
            unique_lock<std::mutex> lock(queueMutex);
            workers[id].cv.wait(lock, [this, id] { return stop || workers[id].busy; });
            if (stop && !workers[id].busy) { // Modificación: Condición de terminación mejorada
                break;
            }
            task = move(workers[id].task); // Modificación: Usar move
        }

        if (task) {
            task();
            {
                unique_lock<std::mutex> lock(queueMutex);
                workersSemaphore.signal();
                workers[id].busy = false;
                activeWorkers--;
                if (tasks.empty() && activeWorkers == 0) {
                    allTasksDoneCondition.notify_all();
                }
            }
        }
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) 
{
    {
        unique_lock<std::mutex> lock(queueMutex); // lock the queue (adentro de un scope)
        tasks.emplace(thunk); // encola la funcion
        // cout << "Task enqueued" << endl;
    }
    tasksSemaphore.signal();
}

void ThreadPool::wait() // espera que se terminen de ejecutar todas las funciones encoladas (que todas las tareas hayan terminado con el trabajo que esten y esten availables) 
{
    unique_lock<std::mutex> lock(queueMutex);
    allTasksDoneCondition.wait(lock, [this] { return tasks.empty() && activeWorkers == 0; }); // espera a que no haya tareas encoladas y que no haya workers activos
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

       