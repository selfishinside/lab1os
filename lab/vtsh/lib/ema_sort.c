#define _POSIX_C_SOURCE 200809L
#include "ema_sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

typedef struct {
    FILE* file;
    int32_t current_value;
    int is_empty;
} RunFile;

typedef struct {
    RunFile* runs;
    int count;
} RunHeap;

static int compare_integers(const void* a, const void* b) {
    int32_t x = *(const int32_t*)a;
    int32_t y = *(const int32_t*)b;
    return (x > y) - (x < y);
}

static void swap_runs(RunFile* array, int i, int j) {
    RunFile temp = array[i];
    array[i] = array[j];
    array[j] = temp;
}

static int read_next_value(RunFile* run) {
    if (fread(&run->current_value, sizeof(int32_t), 1, run->file) != 1) {
        run->is_empty = 1;
        return 0;
    }
    run->is_empty = 0;
    return 1;
}

static void heapify(RunHeap* heap, int index) {
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int smallest = index;

        if (left < heap->count && !heap->runs[left].is_empty &&
            (heap->runs[smallest].is_empty || heap->runs[left].current_value < heap->runs[smallest].current_value))
            smallest = left;
        if (right < heap->count && !heap->runs[right].is_empty &&
            (heap->runs[smallest].is_empty || heap->runs[right].current_value < heap->runs[smallest].current_value))
            smallest = right;
        if (smallest == index) break;
        swap_runs(heap->runs, index, smallest);
        index = smallest;
    }
}

int ema_sort_integers(const char* input_path, const char* output_path, size_t memory_limit_mb) {
    FILE* input_file = fopen(input_path, "rb");
    if (!input_file) {
        perror("Failed to open input file");
        return 1;
    }

    size_t max_elements = (memory_limit_mb * 1024ull * 1024ull) / sizeof(int32_t);
    if (max_elements < 1024) max_elements = 1024;

    int32_t* buffer = malloc(max_elements * sizeof(int32_t));
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(input_file);
        return 1;
    }

    char** temp_filenames = NULL;
    size_t run_count = 0, run_capacity = 0;

    while (1) {
        size_t read_count = fread(buffer, sizeof(int32_t), max_elements, input_file);
        if (read_count == 0) break;

        qsort(buffer, read_count, sizeof(int32_t), compare_integers);

        char temp_name[] = "/tmp/ema_runXXXXXX";
        int temp_fd = mkstemp(temp_name);
        FILE* temp_file = fdopen(temp_fd, "wb+");

        fwrite(buffer, sizeof(int32_t), read_count, temp_file);
        fflush(temp_file);
        fseek(temp_file, 0, SEEK_SET);

        if (run_count == run_capacity) {
            run_capacity = run_capacity ? run_capacity * 2 : 8;
            temp_filenames = realloc(temp_filenames, run_capacity * sizeof(char*));
        }
        temp_filenames[run_count++] = strdup(temp_name);
    }
    fclose(input_file);
    free(buffer);

    FILE* output_file = fopen(output_path, "wb");
    RunFile* runs = calloc(run_count, sizeof(RunFile));

    for (size_t i = 0; i < run_count; i++) {
        runs[i].file = fopen(temp_filenames[i], "rb");
        read_next_value(&runs[i]);
    }

    RunHeap heap = { .runs = runs, .count = (int)run_count };
    for (int i = heap.count / 2; i >= 0; --i) heapify(&heap, i);

    while (heap.count > 0) {
        if (heap.runs[0].is_empty) {
            if (heap.count > 1) swap_runs(heap.runs, 0, heap.count - 1);
            fclose(heap.runs[heap.count - 1].file);
            heap.count--;
            heapify(&heap, 0);
            continue;
        }
        int32_t value = heap.runs[0].current_value;
        fwrite(&value, sizeof(value), 1, output_file);
        if (!read_next_value(&heap.runs[0])) {}
        heapify(&heap, 0);
    }

    fclose(output_file);
    for (size_t i = 0; i < run_count; i++) {
        remove(temp_filenames[i]);
        free(temp_filenames[i]);
    }
    free(temp_filenames);
    free(runs);
    return 0;
}
