#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <queue>
#include <random>


class TaskQueue {
    private:
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mtx;
    int totalExecutionTime;
    const int maxTime;

    public:
    TaskQueue() : totalExecutionTime(0), maxTime(60) {};

    ~TaskQueue() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        while (!tasks.empty()) {
            tasks.pop();
        }
    };

    bool addTask(const std::function<void()>& task, int estimatedTime) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (estimatedTime + totalExecutionTime > maxTime) {
            std::cout << "Rejected: task " << estimatedTime << "seconds" << std::endl;
            return false;
        }
        tasks.push(task);
        totalExecutionTime += estimatedTime;
        std::cout << "Task added with estimated time: " << estimatedTime << "seconds" << std::endl;
        return true;
    }

    bool getTask(std::function<void()>& task) {
        std::lock_guard<std::mutex> lock(queue_mtx);
        if (tasks.empty()) return false;
        task = tasks.front();
        tasks.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(queue_mtx);
        std::queue<std::function<void()>> empty;
        swap(tasks, empty);
        totalExecutionTime = 0;
    }
};

class ThreadPool {

};

int main() {

    return 0;
}