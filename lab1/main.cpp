#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace std;

const int N = 5; // Розмір квадратної матриці

void fillMatrix(int matrix[N][N]) {
    srand(time(0));
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            matrix[i][j] = rand() % 100;
        }
    }
}

void printMatrix(int matrix[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            cout << matrix[i][j] << "\t";
        }
        cout << endl;
    }
    cout << endl;
}

void mirrorMatrix(int matrix[N][N]) {
    int mirrored[N][N];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            mirrored[i][j] = matrix[N - j - 1][N - i - 1];
        }
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            matrix[i][j] = mirrored[i][j];
        }
    }
}

int main() {
    int matrix[N][N];

    fillMatrix(matrix);
    cout << "First matrix:" << endl;
    printMatrix(matrix);

    mirrorMatrix(matrix);
    cout << "Matrix:" << endl;
    printMatrix(matrix);

    return 0;
}