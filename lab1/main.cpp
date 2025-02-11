#include <iostream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <vector>

using namespace std;

const int N = 1000; // Розмір квадратної матриці
const int M = 1000;
const int THREADS = 4; // Кількість потоків

int** allocateMatrix(int size) {
    int** matrix = new int*[size];
    for (int i = 0; i < size; i++) {
        matrix[i] = new int[size];
    }
    return matrix;
}

void deallocateMatrix(int** matrix, int size) {
    for (int i = 0; i < size; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

void fillMatrixPart(int** matrix, int size, int startRow, int endRow) {
    srand(time(0) + startRow); // Додаємо відмінність для кожного потоку
    for (int i = startRow; i < endRow; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = rand() % 100;
        }
    }
}

void fillMatrix(int** matrix, int size) {
    vector<thread> threads;
    int rowsPerThread = size / THREADS;

    for (int t = 0; t < THREADS; t++) {
        int startRow = t * rowsPerThread;
        int endRow = (t == THREADS - 1) ? size : (t + 1) * rowsPerThread;
        threads.push_back(thread(fillMatrixPart, matrix, size, startRow, endRow));
    }

    for (auto& t : threads) {
        t.join();
    }
}

void mirrorMatrixPart(int** matrix, int size, int startRow, int endRow, int** mirrored) {
    for (int i = startRow; i < endRow; i++) {
        for (int j = 0; j < size; j++) {
            mirrored[i][j] = matrix[size - j - 1][size - i - 1];
        }
    }
}

void mirrorMatrix(int** matrix, int size) {
    int** mirrored = allocateMatrix(size);
    vector<thread> threads;
    int rowsPerThread = size / THREADS;

    for (int t = 0; t < THREADS; t++) {
        int startRow = t * rowsPerThread;
        int endRow = (t == THREADS - 1) ? size : (t + 1) * rowsPerThread;
        threads.push_back(thread(mirrorMatrixPart, matrix, size, startRow, endRow, mirrored));
    }

    for (auto& t : threads) {
        t.join();
    }

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = mirrored[i][j];
        }
    }

    deallocateMatrix(mirrored, size);
}

void printMatrix(int** matrix, int size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            cout << matrix[i][j] << "\t";
        }
        cout << endl;
    }
    cout << endl;
}

int main() {
    int** matrix = allocateMatrix(N);

    clock_t startTime = clock();
    for (int iter = 0; iter < M; iter++) {
        fillMatrix(matrix, N);
        mirrorMatrix(matrix, N);
    }
    clock_t endTime = clock();
    double seconds = (double(endTime - startTime)) / CLOCKS_PER_SEC;
    cout << "For " << M << " repetitions: " << seconds << " seconds" << endl;

    /*fillMatrix(matrix, N);
    cout << "First matrix:" << endl;
    printMatrix(matrix, N);

    mirrorMatrix(matrix, N);
    cout << "Mirrored matrix:" << endl;
    printMatrix(matrix, N);

    deallocateMatrix(matrix, N);*/
    return 0;
}
