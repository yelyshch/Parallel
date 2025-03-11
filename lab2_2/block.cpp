#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>
#include <random>

using namespace std;
using namespace chrono;

mutex mtx;

void generateRandomNumbers(vector<int>& numbers, int min_val, int max_val, int startIdx, int endIdx, int thread_id) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count() + thread_id);
    uniform_int_distribution<int> dist(min_val, max_val);
    for (int i = startIdx; i < endIdx; ++i) {
        numbers[i] = dist(rng);
    }
}

void findMultiplesOf17(const vector<int>& numbers, int startIdx, int endIdx, int& count, int& min_element) {
    int local_count = 0;
    int local_min_element = -1;

    for (int i = startIdx; i < endIdx; ++i) {
        if (numbers[i] % 17 == 0) {
            local_count++;
            if (local_min_element == -1 || numbers[i] < local_min_element) {
                local_min_element = numbers[i];
            }
        }
    }

    lock_guard<mutex> lock(mtx);
    count += local_count;
    if (local_min_element != -1 && (min_element == -1 || local_min_element < min_element)) {
        min_element = local_min_element;
    }
}

void printResults(int count, int min_element) {
    if (count > 0) {
        cout << "Number of elements divisible by 17: " << count << endl;
        cout << "Smallest element divisible by 17: " << min_element << endl;
    } else {
        cout << "No elements divisible by 17 found." << endl;
    }
}

int main() {
    const int NUM_THREADS = 6;
    const int ARRAY_SIZE = 20;
    vector<int> numbers(ARRAY_SIZE);
    int count = 0;
    int min_element = -1;

    auto startTime = high_resolution_clock::now();

    vector<thread> threads;
    int chunkSize = ARRAY_SIZE / NUM_THREADS;
    int remainder = ARRAY_SIZE % NUM_THREADS;

    int startIdx = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        int endIdx = startIdx + chunkSize + (i < remainder ? 1 : 0);
        threads.emplace_back(generateRandomNumbers, ref(numbers), 10, 1000000, startIdx, endIdx, i);
        startIdx = endIdx;
    }

    for (auto& t : threads) {
        t.join();
    }

    threads.clear();
    startIdx = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        int endIdx = startIdx + chunkSize + (i < remainder ? 1 : 0);
        threads.emplace_back(findMultiplesOf17, cref(numbers), startIdx, endIdx, ref(count), ref(min_element));
        startIdx = endIdx;
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(endTime - startTime);

    printResults(count, min_element);
    cout << "Time: " << duration.count() << " microseconds" << endl;

    for (int num : numbers) {
        cout << num << " ";
    }
    cout << endl;

    return 0;
}
