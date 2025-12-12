#define _POSIX_C_SOURCE 200809L
#include "vtsh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char line[1024];
    int interactive = isatty(STDIN_FILENO);
    int nesting = getenv("VTSH_NESTED") != NULL;

    if (nesting) interactive = 0;

    if (!interactive) {
        // Считаем весь stdin одним куском (как в vtsh_loop), чтобы поддерживать
        // поведение: первая строка может быть команда 'cat' и далее идёт ввод.
        char full_input[8192] = {0};
        size_t len = fread(full_input, 1, sizeof(full_input) - 1, stdin);
        if (len == 0)
            return 0;
        // Если во входе только один перевод строки — это пустая команда, выходим
        if (len == 1 && full_input[0] == '\n')
            return 0;

        // Разделяем первую строку и остаток
        char *newline = strchr(full_input, '\n');
        if (!newline) {
            vtsh_eval(full_input);
            return 0;
        }

        *newline = '\0';
        char *first_line = full_input;
        char *rest = newline + 1;

        // Если первая команда — cat без аргументов → печатаем остальное
        if (strcmp(first_line, "cat") == 0) {
            fputs(rest, stdout);
            fflush(stdout);
            return 0;
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
        return 0;
    }

    while (1) {
        printf("%s", vtsh_prompt());
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0) break;
        if (line[0] == '\0') continue;

        vtsh_eval(line);
    }

    return 0;
}
