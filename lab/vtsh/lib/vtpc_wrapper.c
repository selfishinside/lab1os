#include "vtpc_wrapper.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

/* Импортируем vtpc API если USE_VTPC=1 */
#ifdef USE_VTPC
extern int vtpc_open(const char* path, int mode, int access);
extern int vtpc_close(int fd);
extern ssize_t vtpc_read(int fd, void* buf, size_t count);
extern ssize_t vtpc_write(int fd, const void* buf, size_t count);
extern off_t vtpc_lseek(int fd, off_t offset, int whence);
extern int vtpc_fsync(int fd);
#endif

static int use_vtpc(void) {
    static int checked = 0;
    static int result = 0;
    if (!checked) {
        const char *e = getenv("USE_VTPC");
        result = (e && strcmp(e, "1") == 0);
        checked = 1;
    }
    return result;
}

VtpcFile vtpc_wrap_open(const char *path, int flags, int mode) {
    VtpcFile f = {0};
#ifdef USE_VTPC
    if (use_vtpc()) {
        f.fd = vtpc_open(path, flags, mode);
        f.is_vtpc = (f.fd >= 0) ? 1 : 0;
        return f;
    }
#endif
    f.fd = open(path, flags, mode);
    f.is_vtpc = 0;
    return f;
}

int vtpc_wrap_close(VtpcFile f) {
#ifdef USE_VTPC
    if (f.is_vtpc) return vtpc_close(f.fd);
#endif
    return close(f.fd);
}

ssize_t vtpc_wrap_read(VtpcFile f, void *buf, size_t count) {
#ifdef USE_VTPC
    if (f.is_vtpc) return vtpc_read(f.fd, buf, count);
#endif
    return read(f.fd, buf, count);
}

ssize_t vtpc_wrap_write(VtpcFile f, const void *buf, size_t count) {
#ifdef USE_VTPC
    if (f.is_vtpc) return vtpc_write(f.fd, (void*)buf, count);
#endif
    return write(f.fd, buf, count);
}

off_t vtpc_wrap_lseek(VtpcFile f, off_t offset, int whence) {
#ifdef USE_VTPC
    if (f.is_vtpc) return vtpc_lseek(f.fd, offset, whence);
#endif
    return lseek(f.fd, offset, whence);
}

int vtpc_wrap_fsync(VtpcFile f) {
#ifdef USE_VTPC
    if (f.is_vtpc) return vtpc_fsync(f.fd);
#endif
    return fsync(f.fd);
}
