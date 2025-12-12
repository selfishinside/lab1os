#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cpu_mat_mul.h"

static double get_current_time_seconds(void) {
    struct timespec time_now;
    clock_gettime(CLOCK_MONOTONIC, &time_now);
    return time_now.tv_sec + time_now.tv_nsec * 1e-9;
}

int perform_matrix_multiplication(int matrix_size, int repetitions, unsigned int seed) {
    srand(seed);
    double *matrix_a = aligned_alloc(64, sizeof(double) * matrix_size * matrix_size);
    double *matrix_b = aligned_alloc(64, sizeof(double) * matrix_size * matrix_size);
    double *matrix_c = aligned_alloc(64, sizeof(double) * matrix_size * matrix_size);

    if (!matrix_a || !matrix_b || !matrix_c) {
        perror("Memory allocation failed");
        return 1;
    }

    for (int i = 0; i < matrix_size * matrix_size; i++) {
        matrix_a[i] = (rand() % 1000) / 1000.0;
        matrix_b[i] = (rand() % 1000) / 1000.0;
    }

    double start_time = get_current_time_seconds();

    for (int repeat_index = 0; repeat_index < repetitions; repeat_index++) {
        memset(matrix_c, 0, sizeof(double) * matrix_size * matrix_size);
        for (int row = 0; row < matrix_size; row++) { //j
            for (int k = 0; k < matrix_size; k++) { //k
                double value = matrix_a[row * (long)matrix_size + k]; //a[j][k]
                for (int col = 0; col < matrix_size; col++) { //j
                    matrix_c[row * (long)matrix_size + col] += value * matrix_b[k * (long)matrix_size + col]; //C[i][j] += A[i][k] * B[k][j]
                }
            }
        }
    }

    double end_time = get_current_time_seconds();
    double checksum = 0;
    for (int i = 0; i < matrix_size * matrix_size; i++) {
        checksum += matrix_c[i];
    }

    printf("Matrix size: %d, repetitions: %d, elapsed time: %.3f s, checksum: %.6f\n",
           matrix_size, repetitions, end_time - start_time, checksum);

    free(matrix_a);
    free(matrix_b);
    free(matrix_c);
    return 0;
}

int main(int argc, char** argv) {
    int matrix_size = 512;
    int repetitions = 1;
    unsigned int seed = 123;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) matrix_size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-r") && i + 1 < argc) repetitions = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (unsigned int)atoi(argv[++i]);
    }

    return perform_matrix_multiplication(matrix_size, repetitions, seed);
}
