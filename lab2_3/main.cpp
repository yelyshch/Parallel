#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <random>
#include <chrono>

using namespace std;
using namespace chrono;

const int ARRAY_SIZE = 20;
const int MIN_VAL = 0;
const int MAX_VAL = 10000;
const int NUM_THREADS = 6;

vector<atomic<int>> random_numbers(ARRAY_SIZE);
atomic<int> count_17(0);
atomic<int> min_val(MAX_VAL + 1);

void generateRandomNumbers(int start, int end, int seed) {
    mt19937 rng(seed);
    uniform_int_distribution<int> dist(MIN_VAL, MAX_VAL);
    for (int i = start; i < end; i++) {
        int random_value = dist(rng);
        random_numbers[i].store(random_value, memory_order_relaxed);
    }
}

void findMultiplesOf17(int start, int end) {
    for (int i = start; i < end; i++) {
        int value = random_numbers[i].load(memory_order_relaxed);
        if (value % 17 == 0) {
            count_17.fetch_add(1, memory_order_relaxed);
            int current_min = min_val.load(memory_order_relaxed);
            while (value < current_min &&
                   !min_val.compare_exchange_weak(current_min, value, memory_order_relaxed));
        }
    }
}

void printResults() {
    int final_min = min_val.load(memory_order_relaxed);
    if (count_17.load(memory_order_relaxed) > 0 && final_min <= MAX_VAL) {
        cout << "Number of elements divisible by 17: " << count_17.load(memory_order_relaxed) << endl;
        cout << "Minimum of those elements: " << final_min << endl;
    } else {
        cout << "No elements divisible by 17 found." << endl;
    }
}

int main() {
    vector<thread> threads;

    auto startTime = high_resolution_clock::now();

    int chunk_size = ARRAY_SIZE / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        int start = i * chunk_size;
        int end = (i == NUM_THREADS - 1) ? ARRAY_SIZE : start + chunk_size;
        threads.emplace_back(generateRandomNumbers, start, end, rand());
    }

    for (auto& t : threads) t.join();
    threads.clear();

    for (int i = 0; i < NUM_THREADS; i++) {
        int start = i * chunk_size;
        int end = (i == NUM_THREADS - 1) ? ARRAY_SIZE : start + chunk_size;
        threads.emplace_back(findMultiplesOf17, start, end);
    }

    for (auto& t : threads) t.join();

    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(endTime - startTime);

    printResults();
    cout << "Time: " << duration.count() << " microseconds" << endl;

    for (int i = 0; i < ARRAY_SIZE; i++) {
        cout << random_numbers[i].load(memory_order_relaxed) << " ";
    }
    cout << endl;

    return 0;
}
