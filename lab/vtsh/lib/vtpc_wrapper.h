#pragma once

#include <sys/types.h>

/**
 * Обёртка для переключения между libc и vtpc
 * Если переменная окружения USE_VTPC=1, используется vtpc_*
 * Иначе используются стандартные системные вызовы open/read/write
 */

typedef struct {
    int fd;
    int is_vtpc;  // 1 если используется vtpc, 0 если libc (open/read/write)
} VtpcFile;

// Открыть файл (выбирает между vtpc_open и open)
VtpcFile vtpc_wrap_open(const char *path, int flags, int mode);

// Закрыть файл
int vtpc_wrap_close(VtpcFile f);

// Читать из файла
ssize_t vtpc_wrap_read(VtpcFile f, void *buf, size_t count);

// Писать в файл
ssize_t vtpc_wrap_write(VtpcFile f, const void *buf, size_t count);

// Искать в файле
off_t vtpc_wrap_lseek(VtpcFile f, off_t offset, int whence);

// Синхронизировать
int vtpc_wrap_fsync(VtpcFile f);
