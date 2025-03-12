#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>  // Micro

using namespace std;
using namespace chrono;

const int SIZE = 1000000;
const int MIN= 10;
const int MAX = 10000;

void generateRandomNumbers(vector<int>& numbers, int size, int min_val, int max_val) {
    numbers.clear();
    for (int i = 0; i < size; i++) {
        numbers.push_back(min_val + rand() % (max_val - min_val + 1));
    }
}

void findMultiplesOf17(const vector<int>& numbers, int& count, int& min_element) {
    count = 0;
    min_element = -1;

    for (int num : numbers) {
        if (num % 17 == 0) {
            count++;
            if (min_element == -1 || num < min_element) {
                min_element = num;
            }
        }
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
    vector<int> numbers;
    int count, min_element;

    auto startTime = high_resolution_clock::now();

    generateRandomNumbers(numbers, SIZE, MIN, MAX);
    findMultiplesOf17(numbers, count, min_element);

    auto endTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(endTime - startTime);

    printResults(count, min_element);
    cout << "Time: " << duration.count() << " microseconds" << endl;

    /*for(int i = 1; i < SIZE; i++) {
        cout << numbers[i] << " ";
    }
    cout << endl;*/

    return 0;
}
