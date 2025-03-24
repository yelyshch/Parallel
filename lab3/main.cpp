#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>
#include <fstream>
#include <ctime>

struct Task {
    int id;
    int duration;
    std::function<void()> func;

    Task(int id, int duration, std::function<void()> func)
        : id(id), duration(duration), func(std::move(func)) {}
};

class TaskQueue {
private:
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    int totalExecutionTime;
    const int maxTime;
    int rejectedTasks = 0;

public:
    TaskQueue() : totalExecutionTime(0), maxTime(60) {}

    ~TaskQueue() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::queue<Task>().swap(tasks);
    }

    bool addTask(int id, int estimatedTime, const std::function<void()>& taskFunc) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (estimatedTime + totalExecutionTime > maxTime) {
            std::cout << "[TaskQueue] Rejected: task " << id << " (" << estimatedTime << " seconds)\n";
            rejectedTasks++;
            return false;
        }
        tasks.emplace(id, estimatedTime, taskFunc);
        totalExecutionTime += estimatedTime;
        std::cout << "[TaskQueue] Task " << id << " added with estimated time: " << estimatedTime << " seconds\n";
        return true;
    }

    bool getTask(Task& task) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (tasks.empty()) return false;
        task = tasks.front();
        tasks.pop();
        return true;
    }

    int getRejectedTaskCount() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        return rejectedTasks;
    }

    void swap(TaskQueue& other) noexcept {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::lock_guard<std::mutex> other_lock(other.queue_mtx);
        std::swap(tasks, other.tasks);
        std::swap(totalExecutionTime, other.totalExecutionTime);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::queue<Task>().swap(tasks);
        totalExecutionTime = 0;
        std::cout << "[TaskQueue] Queue cleared.\n";
    }

    int getTaskCount() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        return tasks.size();
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
    int completedTasks = 0;
    int swapCount = 0;

public:
    explicit ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i] {
                while (true) {
                    Task task(0, 0, []{});
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this, &task] { return stop || mainQueue.getTask(task); });
                        if (stop) return;
                    }
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        std::cout << "[Worker " << i << "] Executing task ID: " << task.id << " (" << task.duration << " seconds)\n";
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(task.duration));
                    task.func();
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        std::cout << "[Worker " << i << "] Task ID: " << task.id << " completed.\n";
                        completedTasks++;
                    }
                }
            });
        }
    }

    void scheduleExecution() {
        while (!stop) {
            std::this_thread::sleep_for(std::chrono::seconds(40));
            {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << "[Scheduler] Swapping queues and notifying workers.\n";
                mainQueue.swap(bufferQueue);
                bufferQueue.clear();
                swapCount++;
            }
            logMetrics();
            cv.notify_all();
        }
    }

    void addTask(int id, int estimatedTime, const std::function<void()>& taskFunc) {
        bufferQueue.addTask(id, estimatedTime, taskFunc);
    }

    void logMetrics() {
        std::ofstream logFile("metrics.log", std::ios::app);
        if (!logFile) return;

        std::time_t now = std::time(nullptr);
        logFile << "Time: " << std::ctime(&now)
                << "Swap: " << swapCount
                << " | Total Tasks: " << bufferQueue.getTaskCount() + completedTasks
                << " | Rejected Tasks: " << bufferQueue.getRejectedTaskCount()
                << " | Completed Tasks: " << completedTasks << "\n";
        logFile.close();
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
        pool.addTask(i, execTime, [i, execTime]() {
            std::this_thread::sleep_for(std::chrono::seconds(execTime));
        });
        ++i;
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    scheduler.join();
    return 0;
}
