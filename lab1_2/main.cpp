#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace std;

const int N = 1000; // Розмір квадратної матриці
const int M = 1000;

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

void fillMatrix(int** matrix, int size) {
    srand(time(0));
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = rand() % 100;
        }
    }
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

void mirrorMatrix(int** matrix, int size) {
    int** mirrored = allocateMatrix(size);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            mirrored[i][j] = matrix[size - j - 1][size - i - 1];
        }
    }

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = mirrored[i][j];
        }
    }
    deallocateMatrix(mirrored, size);
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
    printMatrix(matrix, N);*/

    deallocateMatrix(matrix, N);
    return 0;
}
