// bin/proc_vfork.c
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s -n <num_processes>\n", prog);
}

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char **argv) {
    long count = 1000;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            count = atol(argv[++i]);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (count <= 0) {
        usage(argv[0]);
        return 1;
    }

    long long t0 = now_ns();

    long started = 0, ok = 0;

    for (long i = 0; i < count; ++i) {
        pid_t pid = vfork();
        if (pid < 0) {
            perror("vfork");
            break;
        }

        if (pid == 0) {
            _exit(0);
        }

        started++;

        int st = 0;
        if (waitpid(pid, &st, 0) > 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0) {
            ok++;
        }
    }

    long long t1 = now_ns();
    double sec = (t1 - t0) / 1e9;

    printf(
        "proc-vfork: count=%ld started=%ld ok=%ld time=%.6f s avg=%.6f ms/fork\n",
        count,
        started,
        ok,
        sec,
        (started > 0 ? (sec * 1000.0) / started : 0.0)
    );

    return 0;
}