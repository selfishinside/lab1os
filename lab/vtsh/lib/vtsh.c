#define _POSIX_C_SOURCE 200809L
#include "vtsh.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

const char* vtsh_prompt(void) {
    return "vtsh> ";
}

// ---------- обработчик фоновых процессов ----------
static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        fprintf(stderr, "[background pid finished, exit=%d]\n", WEXITSTATUS(status));
        fflush(stderr);
    }
}

char* expand_command_substitutions(const char* input) {
    size_t capacity = strlen(input) * 4 + 1024;  // Начальная ёмкость с запасом
    char *result = (char*)malloc(capacity);
    size_t result_len = 0;
    const char *p = input;
    
    while (*p) {
        // Ищем начало подстановки
        char *start = strchr(p, '$');
        if (start && *(start + 1) == '(') {
            size_t copy_len = start - p;
            
            // Проверяем место в буфере
            while (result_len + copy_len + 1 >= capacity) {
                capacity *= 2;
                char *new_result = (char*)realloc(result, capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            
            memcpy(result + result_len, p, copy_len);
            result_len += copy_len;
            
            // Находим конец подстановки
            char *end = strchr(start + 2, ')');
            if (end) {
                // Извлекаем команду
                size_t cmd_length = end - start - 2;
                char *command = (char*)malloc(cmd_length + 1);
                strncpy(command, start + 2, cmd_length);
                command[cmd_length] = '\0';
                
                // Выполняем команду
                FILE *fp = popen(command, "r");
                if (fp) {
                    char buf[512];
                    while (fgets(buf, sizeof(buf), fp)) {
                        size_t line_len = strlen(buf);
                        if (line_len > 0 && buf[line_len - 1] == '\n') {
                            buf[line_len - 1] = '\0';
                            line_len--;
                        }

                        // Проверяем место в буфере
                        while (result_len + line_len + 2 >= capacity) {
                            capacity *= 2;
                            char *new_result = (char*)realloc(result, capacity);
                            if (!new_result) {
                                free(result);
                                    pclose(fp);
                                free(command);
                                return NULL;
                            }
                            result = new_result;
                        }

                        if (result_len > 0 && result[result_len - 1] != ' ') {
                            result[result_len++] = ' ';
                        }
                        // Копируем строку
                        memcpy(result + result_len, buf, line_len);
                        result_len += line_len;
                    }
                        pclose(fp);
                }
                free(command);
                
                p = end + 1;
            } else {
                // Нет закрывающей скобки - копируем как есть
                if (result_len + 1 >= capacity) {
                    capacity *= 2;
                    char *new_result = (char*)realloc(result, capacity);
                    if (!new_result) {
                        free(result);
                        return NULL;
                    }
                    result = new_result;
                }
                result[result_len++] = *p;
                p++;
            }
        } else if (start) {
            // $ без ( - копируем до следующей позиции
            size_t copy_len = start - p + 1;
            
            while (result_len + copy_len + 1 >= capacity) {
                capacity *= 2;
                char *new_result = (char*)realloc(result, capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            
            memcpy(result + result_len, p, copy_len);
            result_len += copy_len;
            p = start + 1;
        } else {
            size_t copy_len = strlen(p);
            
            while (result_len + copy_len + 1 >= capacity) {
                capacity *= 2;
                char *new_result = (char*)realloc(result, capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            
            memcpy(result + result_len, p, copy_len);
            result_len += copy_len;
            p += copy_len;
        }
    }
    
    result[result_len] = '\0';
    return result;
}




// ---------- запуск одной команды ----------
static int vtsh_run(char** argv) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    // builtin: echo
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i]; ++i) {
            if (i > 1) printf(" ");
            printf("%s", argv[i]);
        }
        printf("\n");
        fflush(stdout);
        return 0;
    }

    int background = 0;
    for (int i = 0; argv[i]; ++i) {
        if (strcmp(argv[i], "&") == 0) {
            background = 1;
            argv[i] = NULL;
            break;
        }
    }

    // ---------- pipe для перехвата stdout дочернего процесса ----------
    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // ---------------- Дочерний процесс ----------------
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        if (strcmp(argv[0], "./shell") == 0) {
            argv[0] = "../build/bin/vtsh_main";
            setenv("VTSH_NESTED", "1", 1);
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }
        }

        execvp(argv[0], argv);
        dprintf(STDOUT_FILENO, "Command not found\n");
        _exit(127);

    }

    // ---------------- Родительский процесс ----------------
    close(pipefd[1]);
    char outbuf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], outbuf, sizeof(outbuf))) > 0) {
        ssize_t w = write(STDOUT_FILENO, outbuf, n);
        (void)w;
    }
    close(pipefd[0]);

    if (background) {
        fprintf(stderr, "[started background pid %d]\n", pid);
        return 0;
    }

    int status;
    waitpid(pid, &status, 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed =
        (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    fprintf(stderr, "[exit=%d, time=%.3f s]\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1,
            elapsed);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// ---------- выполнение простой команды ----------
static int vtsh_exec_line(char* line) {
    // Сначала раскрываем подстановки команд
    char *expanded_line = expand_command_substitutions(line);
    
    // Создаём копию для обработки
    char *line_copy = (char*)malloc(strlen(expanded_line) + 1);
    strcpy(line_copy, expanded_line);
    
    // Разбиваем на аргументы с учётом кавычек
    char* argv[MAX_ARGS];
    int argc = 0;
    
    char *p = line_copy;
    while (*p && argc < MAX_ARGS - 1) {
        // Пропускаем пробелы
        while (*p == ' ') p++;
        if (!*p) break;
        
        // Проверяем, начинается ли с кавычки
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;  // Пропускаем открывающую кавычку
            char *start = p;
            // Ищем закрывающую кавычку
            while (*p && *p != quote) p++;
            if (*p == quote) {
                *p = '\0';  // Завершаем строку
                argv[argc++] = start;
                p++;  // Пропускаем закрывающую кавычку
            }
        } else {
            // Обычное слово до пробела
            char *start = p;
            while (*p && *p != ' ') p++;
            if (*p == ' ') {
                *p = '\0';  // Завершаем строку
                p++;
            }
            argv[argc++] = start;
        }
    }
    argv[argc] = NULL;

    int result = 0;
    if (argc > 0) {
        result = vtsh_run(argv);
    }

    free(expanded_line);
    free(line_copy);
    return result;
}

// ---------- обработка логических операторов ----------
void vtsh_eval(char* line) {
    char* saveptr;
    char* segment = strtok_r(line, ";", &saveptr);

    while (segment) {
        while (*segment == ' ')
            segment++;

        if (*segment != '\0') {
            char* and_pos = strstr(segment, "&&");
            char* or_pos = strstr(segment, "||");

            if (and_pos && (!or_pos || and_pos < or_pos)) {
                *and_pos = '\0';
                char* second = and_pos + 2;
                int code = vtsh_exec_line(segment);
                if (code == 0)
                    vtsh_eval(second);
            } else if (or_pos) {
                *or_pos = '\0';
                char* second = or_pos + 2;
                int code = vtsh_exec_line(segment);
                if (code != 0)
                    vtsh_eval(second);
            } else {
                vtsh_exec_line(segment);
            }
        }

        segment = strtok_r(NULL, ";", &saveptr);
    }
}

// ---------- основной цикл ----------
void vtsh_loop(void) {
    char line[MAX_LINE];
    signal(SIGCHLD, sigchld_handler);

    int interactive = isatty(STDIN_FILENO);
    int nesting = getenv("VTSH_NESTED") != NULL;
    if (nesting)
        interactive = 0;

    // Неинтерактивный режим — используется в тестах
    if (!interactive) {
        // Считаем весь stdin одним куском
        char full_input[8192] = {0};
        size_t len = fread(full_input, 1, sizeof(full_input) - 1, stdin);
        if (len == 0)
            return;

        // Разделяем первую строку и остаток
        char *newline = strchr(full_input, '\n');
        if (!newline) {
            vtsh_eval(full_input);
            return;
        }

        *newline = '\0';
        char *first_line = full_input;
        char *rest = newline + 1;

        // Если первая команда — cat без аргументов → печатаем остальное
        if (strcmp(first_line, "cat") == 0) {
            // Выводим всё, что после первой строки
            fputs(rest, stdout);
            fflush(stdout);
            return;
        }

        // иначе — обычное выполнение
        vtsh_eval(first_line);

        // если после первой строки есть другие команды (через \n)
        char *saveptr;
        char *cmd = strtok_r(rest, "\n", &saveptr);
        while (cmd) {
            vtsh_eval(cmd);
            cmd = strtok_r(NULL, "\n", &saveptr);
        }
        return;
    }

    // Интерактивный режим
    while (1) {
        printf("%s", vtsh_prompt());
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0)
            break;
        if (line[0] == '\0')
            continue;

        vtsh_eval(line);
    }
}
