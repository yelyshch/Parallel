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
#include <atomic>

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
    int totalTasks;
    int totalTasksWaiting;

public:
    TaskQueue() : totalExecutionTime(0), maxTime(60), totalTasks(0), totalTasksWaiting(0) {}

    ~TaskQueue() {
        clear();
    }

    bool addTask(int id, int estimatedTime, const std::function<void()>& taskFunc) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        totalTasks++;
        if (estimatedTime + totalExecutionTime > maxTime) {
            std::cout << "[TaskQueue] Rejected: task " << id << " (" << estimatedTime << " seconds)\n";
            return false;
        }
        tasks.emplace(id, estimatedTime, taskFunc);
        totalExecutionTime += estimatedTime;
        std::cout << "[TaskQueue] Task " << id << " added with estimated time: " << estimatedTime << " seconds\n";
        totalTasksWaiting++;
        return true;
    }

    bool getTask(Task& task) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (tasks.empty()) return false;
        task = tasks.front();
        tasks.pop();
        totalExecutionTime -= task.duration;
        return true;
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
        return totalTasks;
    }

    int getTaskWaitingCount() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        return totalTasksWaiting;
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
    bool paused;
    int swapCount = 0;
    int taskCounter = 0;
    std::atomic<int> completedTasks{0};
    std::atomic<int> rejectedTasks{0};

public:
    explicit ThreadPool(size_t numThreads) : stop(false), paused(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this, i] {
                while (true) {
                    Task task(0, 0, []{});
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this, &task] { return stop || (!paused && mainQueue.getTask(task)); });
                        if (stop) return;
                    }
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        std::cout << "[Worker " << i << "] Executing task ID: " << task.id << " (" << task.duration << " seconds)\n";

                    }

                    std::this_thread::sleep_for(std::chrono::seconds(task.duration));
                    completedTasks.fetch_add(1);
                    task.func();
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        std::cout << "[Worker " << i << "] Task ID: " << task.id << " completed.\n";
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
                if (paused) continue;
                std::cout << "[Scheduler] Swapping queues and notifying workers.\n";
                mainQueue.swap(bufferQueue);
                bufferQueue.clear();
                swapCount++;
            }

            logMetrics();
            cv.notify_all();
        }
    }

    void addTask(int estimatedTime, const std::function<void()>& taskFunc) {
        int taskId;
        {
            std::lock_guard<std::mutex> lock(mtx);
            taskId = taskCounter++;
        }
        bool added = bufferQueue.addTask(taskId, estimatedTime, taskFunc);
        if (!added) {
            rejectedTasks.fetch_add(1);
        }
    }

    void pause() {
        std::lock_guard<std::mutex> lock(mtx);
        paused = true;
    }

    void resume() {
        std::lock_guard<std::mutex> lock(mtx);
        paused = false;
        cv.notify_all();
    }

    void shutdown(bool immediate) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
            if (immediate) {
                mainQueue.clear();
                bufferQueue.clear();
            }
        }
        cv.notify_all();
    }

    void logMetrics() {
        std::ofstream logFile("metrics.log", std::ios::app);
        if (!logFile) return;

        std::time_t now = std::time(nullptr);
        logFile << "Time: " << std::ctime(&now)
                << "Swap: " << swapCount
                << " | Total Tasks: " << bufferQueue.getTaskCount() + mainQueue.getTaskCount()
                << " | Rejected Tasks: " << rejectedTasks.load()
                << " | Tasks that must be completed: " << bufferQueue.getTaskWaitingCount() + mainQueue.getTaskWaitingCount()
                << " | Completed Tasks: " << completedTasks.load() << "\n";
        logFile.close();
    }

    ~ThreadPool() {
        shutdown(false);
        for (std::thread &worker : workers) {
            worker.join();
        }
    }
};

void addTasksFromThread(ThreadPool &pool, int numTasks) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(6, 14);

    for (int i = 0; i < numTasks; ++i) {
        int execTime = dist(gen);
        pool.addTask(execTime, [execTime]() {
            std::this_thread::sleep_for(std::chrono::seconds(execTime));
        });
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

int main() {
    ThreadPool pool(4);
    std::thread scheduler(&ThreadPool::scheduleExecution, &pool);

    std::vector<std::thread> adders;
    for (int i = 0; i < 3; ++i) {
        adders.emplace_back(addTasksFromThread, std::ref(pool), 60);
    }

    for (auto& t : adders) {
        t.join();
    }

    pool.shutdown(false);
    scheduler.join();
    return 0;
}
