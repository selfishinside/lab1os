#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ema_sort.h"
#include "ema_sort_int.h"

static void print_usage(const char* program_name) {
    fprintf(stderr,
        "Usage:\n"
        "  %s -i <input_file> -o <output_file> [-m <mem_MiB>]\n\n"
        "Options:\n"
        "  -i   path to input binary file with int32_t values\n"
        "  -o   path to output binary file (sorted result)\n"
        "  -m   memory limit in MiB (default: 128)\n",
        program_name
    );
}

int main(int argc, char** argv) {
    const char* input_path = NULL;
    const char* output_path = NULL;
    size_t memory_limit_mb = 128;

    for (int arg_index = 1; arg_index < argc; arg_index++) {
        if (!strcmp(argv[arg_index], "-i") && arg_index + 1 < argc) {
            input_path = argv[++arg_index];
        } else if (!strcmp(argv[arg_index], "-o") && arg_index + 1 < argc) {
            output_path = argv[++arg_index];
        } else if (!strcmp(argv[arg_index], "-m") && arg_index + 1 < argc) {
            memory_limit_mb = (size_t)atoi(argv[++arg_index]);
        } else if (!strcmp(argv[arg_index], "-h") || !strcmp(argv[arg_index], "--help")) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[arg_index]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!input_path || !output_path) {
        fprintf(stderr, "Error: input and output files are required.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    printf("[ema-sort-int] Input: %s\n", input_path);
    printf("[ema-sort-int] Output: %s\n", output_path);
    printf("[ema-sort-int] Memory limit: %zu MiB\n", memory_limit_mb);

    int result = ema_sort_integers(input_path, output_path, memory_limit_mb);
    if (result == 0) {
        printf("[ema-sort-int] Sorting finished successfully.\n");
    } else {
        fprintf(stderr, "[ema-sort-int] Sorting failed with error code %d.\n", result);
    }
    return result;
}
