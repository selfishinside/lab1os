#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void usage(void) {
  fprintf(stderr, "Usage: shell_seq --cmd=\"/bin/true\" --count=N\n");
}

static long now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

static int split_args(char* line, char** argv, int max) {
  int n = 0;
  char* tok = strtok(line, " ");
  while (tok && n < max - 1) {
    argv[n++] = tok;
    tok = strtok(NULL, " ");
  }
  argv[n] = NULL;
  return n;
}

int main(int argc, char** argv) {
  char* cmdline = NULL;
  long count = 1000;

  static struct option opts[] = {
      {  "cmd", required_argument, 0, 'm'},
      {"count", required_argument, 0, 'c'},
      {      0,                 0, 0,   0}
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "", opts, NULL)) != -1) {
    switch (opt) {
      case 'm':
        cmdline = strdup(optarg);
        break;
      case 'c':
        count = atol(optarg);
        break;
      default:
        usage();
        return 1;
    }
  }
  if (!cmdline)
    cmdline = strdup("/bin/true");
  if (count <= 0) {
    usage();
    return 1;
  }

  long t0 = now_ns();

  long ok = 0;
  for (long i = 0; i < count; i++) {
    // готовим argv для execvp
    char* line = strdup(cmdline);
    if (!line) {
      perror("strdup");
      break;
    }
    char* args[64];
    int n = split_args(line, args, 64);
    if (n == 0) {
      free(line);
      break;
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      free(line);
      break;
    }
    if (pid == 0) {
      execvp(args[0], args);
      perror("execvp");
      _exit(127);
    }

    int st;
    if (waitpid(pid, &st, 0) > 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0) {
      ok++;
    }
    free(line);
  }

  long t1 = now_ns();
  double sec = (t1 - t0) / 1e9;
  printf(
      "shell-seq: cmd=\"%s\" count=%ld ok=%ld time=%.6f s avg=%.6f ms/exec\n",
      cmdline,
      count,
      ok,
      sec,
      (count > 0 ? (sec * 1000.0) / count : 0.0)
  );
  free(cmdline);
  return 0;
}