#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>

struct Task {
    int id;
    int duration;
    std::function<void()> func;
};

class TaskQueue {
private:
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    int totalExecutionTime;
    const int maxTime;

public:
    TaskQueue() : totalExecutionTime(0), maxTime(60) {}

    ~TaskQueue() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        while (!tasks.empty()) {
            tasks.pop();
        }
    }

    bool addTask(int id, int estimatedTime, const std::function<void()>& taskFunc) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (estimatedTime + totalExecutionTime > maxTime) {
            std::cout << "[TaskQueue] Rejected: task " << id << " (" << estimatedTime << " seconds)" << std::endl;
            return false;
        }
        tasks.push({id, estimatedTime, taskFunc});
        totalExecutionTime += estimatedTime;
        std::cout << "[TaskQueue] Task " << id << " added with estimated time: " << estimatedTime << " seconds" << std::endl;
        return true;
    }

    bool getTask(Task& task) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (tasks.empty()) return false;
        task = tasks.front();
        tasks.pop();
        return true;
    }

    void swap(TaskQueue& other) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::lock_guard<std::mutex> other_lock(other.queue_mtx);
        std::swap(tasks, other.tasks);
        std::swap(totalExecutionTime, other.totalExecutionTime);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::queue<Task> empty;
        tasks.swap(empty);
        totalExecutionTime = 0;
        std::cout << "[TaskQueue] Queue cleared." << std::endl;
    }

    void printTasks() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::cout << "[TaskQueue] Tasks to execute: ";
        std::queue<Task> temp = tasks;
        while (!temp.empty()) {
            Task t = temp.front();
            temp.pop();
            std::cout << "[ID: " << t.id << ", Duration: " << t.duration << "s] ";
        }
        std::cout << std::endl;
    }
};

class ThreadPool {
private:
    std::vector<std::thread> workers;
    TaskQueue mainQueue;
    TaskQueue bufferQueue;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;

public:
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i] {
                while (true) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this, &task] { return stop || mainQueue.getTask(task); });
                        if (stop) {
                            std::cout << "[Worker " << i << "] Stopping thread." << std::endl;
                            return;
                        }
                    std::cout << "[Worker " << i << "] Executing task ID: " << task.id << " (" << task.duration << " seconds)" << std::endl;
                    }
                    task.func();
                    std::cout << "[Worker " << i << "] Task ID: " << task.id << " completed." << std::endl;
                }
            });
        }
    }

    void scheduleExecution() {
        while (!stop) {
            std::cout << "[Scheduler] Waiting 40 seconds before executing tasks..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(40));
            {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << "[Scheduler] Swapping queues and notifying workers." << std::endl;
                bufferQueue.printTasks();
                mainQueue.swap(bufferQueue);
                bufferQueue.clear();
            }
            cv.notify_all();
        }
    }

    void addTask(int id, int estimatedTime, const std::function<void()>& taskFunc) {
        if (!bufferQueue.addTask(id, estimatedTime, taskFunc)) {
            std::cout << "[ThreadPool] Task " << id << " rejected due to time limit." << std::endl;
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (std::thread &worker : workers) {
            worker.join();
        }
    }
};

int main() {
    ThreadPool pool(4);
    std::thread scheduler(&ThreadPool::scheduleExecution, &pool);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(6, 14);

    int i = 0;
    while (true) {
        int execTime = dist(gen);
        //std::cout << "[Main] Adding task ID " << i << " with estimated time " << execTime << " seconds" << std::endl;
        pool.addTask(i, execTime, [i, execTime]() {
            std::this_thread::sleep_for(std::chrono::seconds(execTime));
            std::cout << "[Task] Task ID " << i << " executed in " << execTime << " seconds" << std::endl;
        });
        ++i;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    scheduler.join();
    return 0;
}