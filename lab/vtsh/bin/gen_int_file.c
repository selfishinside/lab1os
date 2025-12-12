#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "gen_int_file.h"

int generate_random_integers(const char* output_path, long count, unsigned int seed) {
    FILE* output_file = fopen(output_path, "wb");
    if (!output_file) {
        perror("Failed to open output file");
        return 1;
    }

    srand(seed);
    for (long i = 0; i < count; i++) {
        int32_t value = (int32_t)rand();
        fwrite(&value, sizeof(value), 1, output_file);
    }

    fclose(output_file);
    return 0;
}

int main(int argc, char** argv) {
    const char* output_path = NULL;
    long count = 0;
    unsigned int seed = 123;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) output_path = argv[++i];
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) count = atol(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (unsigned int)atoi(argv[++i]);
    }

    if (!output_path || count <= 0) {
        fprintf(stderr, "Usage: %s -o <file> -n <count> [--seed N]\n", argv[0]);
        return 1;
    }

    return generate_random_integers(output_path, count, seed);
}
