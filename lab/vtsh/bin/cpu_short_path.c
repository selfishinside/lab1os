#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void usage(void) {
  fprintf(stderr, "Usage: cpu_short_path --iters=N [--work=K]\n");
}

static long long now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char** argv) {
  long long iters = 100000000;  // 100 млн итераций
  int work = 10;                // количество простых операций внутри цикла

  static struct option opts[] = {
      {"iters", required_argument, 0, 'i'},
      { "work", required_argument, 0, 'w'},
      {      0,                 0, 0,   0}
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "", opts, NULL)) != -1) {
    switch (opt) {
      case 'i':
        iters = atoll(optarg);
        break;
      case 'w':
        work = atoi(optarg);
        break;
      default:
        usage();
        return 1;
    }
  }

  long long t0 = now_ns();
  double acc = 0.0;

  // чистая вычислительная нагрузка
  for (long long i = 0; i < iters; i++) {
    double x = (double)i / (double)(iters + 1);
    // несколько коротких операций, чтобы нагружать CPU
    for (int j = 0; j < work; j++) {
      acc += sin(x) * cos(x) + sqrt(x + 1.0);
    }
  }

  long long t1 = now_ns();
  double sec = (t1 - t0) / 1e9;
  printf(
      "cpu-short-path: iters=%lld work=%d time=%.3f s, result=%.3f\n",
      iters,
      work,
      sec,
      acc
  );
  return 0;
}