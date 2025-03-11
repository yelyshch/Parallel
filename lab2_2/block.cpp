#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>

using namespace std;
using namespace chrono;

mutex mtx;

void generateRandomNumbers(vector<int>& numbers, int size, int min_val, int max_val, int startIdx, int chunkSize) {
    for (int i = startIdx; i < startIdx + chunkSize && i < size; ++i) {
        numbers[i] = min_val + rand() % (max_val - min_val + 1);
    }
}

void findMultiplesOf17(const vector<int>& numbers, int startIdx, int chunkSize, int& count, int& min_element) {
    int local_count = 0;
    int local_min_element = -1;

    for (int i = startIdx; i < startIdx + chunkSize && i < numbers.size(); ++i) {
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
    srand(time(0));
    vector<int> numbers(1000);
    int count = 0;
    int min_element = -1;

    auto startTime = high_resolution_clock::now();

    vector<thread> threads;
    int chunkSize = numbers.size() / 6;

    for (int i = 0; i < 6; ++i) {
        threads.push_back(thread(generateRandomNumbers, ref(numbers), numbers.size(), 10, 1000000,
            i * chunkSize, chunkSize));
    }

    for (auto& t : threads) {
        t.join();
    }

    threads.clear();

    for (int i = 0; i < 6; ++i) {
        threads.push_back(thread(findMultiplesOf17, cref(numbers), i * chunkSize, chunkSize, ref(count),
            ref(min_element)));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(endTime - startTime);

    printResults(count, min_element);
    cout << "Time: " << duration.count() << " microseconds" << endl;

    return 0;
}
