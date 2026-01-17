#include "vtpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define CACHE_PAGE_COUNT 256 
#define PAGE_SIZE 4096       
#define MAX_OPEN_FILES 32    




typedef struct CachePage {
    int v_fd;                
    off_t offset;            
    char *data;              
    int is_dirty;            
    int is_in_use;           

    struct CachePage *lru_next; 
    struct CachePage *lru_prev; 
} CachePage;


typedef struct {
    int is_open;            
    int sys_fd;              
    off_t seek_pos;          
    off_t file_size;         
} OpenFile;


static CachePage cache_pages[CACHE_PAGE_COUNT];
static void *cache_data_pool = NULL;
static OpenFile open_files[MAX_OPEN_FILES];
static CachePage *lru_head = NULL;
static CachePage *lru_tail = NULL;
static int is_initialized = 0;

// --- Внутренние (static) функции-хелперы ---

static void lru_detach(CachePage *page) {
    if (page->lru_prev) page->lru_prev->lru_next = page->lru_next;
    if (page->lru_next) page->lru_next->lru_prev = page->lru_prev;
    if (lru_head == page) lru_head = page->lru_next;
    if (lru_tail == page) lru_tail = page->lru_prev;
    page->lru_next = page->lru_prev = NULL;
}

static void lru_add_to_head(CachePage *page) {
    page->lru_next = lru_head;
    page->lru_prev = NULL;
    if (lru_head) lru_head->lru_prev = page;
    lru_head = page;
    if (!lru_tail) lru_tail = page;
}

static int flush_page(CachePage *page) {
    if (!page || !page->is_in_use || !page->is_dirty) return 0;
    int sys_fd = open_files[page->v_fd].sys_fd;
    if (pwrite(sys_fd, page->data, PAGE_SIZE, page->offset) == -1) {
        perror("pwrite failed in flush_page");
        return -1;
    }
    page->is_dirty = 0;
    return 0;
}

static int cache_init() {
    if (is_initialized) return 0;
    if (posix_memalign(&cache_data_pool, PAGE_SIZE, CACHE_PAGE_COUNT * PAGE_SIZE) != 0) {
        perror("posix_memalign failed");
        return -1;
    }
    memset(open_files, 0, sizeof(open_files));
    memset(cache_pages, 0, sizeof(cache_pages));
    for (int i = 0; i < CACHE_PAGE_COUNT; ++i) {
        cache_pages[i].data = (char *)cache_data_pool + (i * PAGE_SIZE);
    }
    is_initialized = 1;
    return 0;
}

static CachePage* find_page(int v_fd, off_t page_offset) {
    for (int i = 0; i < CACHE_PAGE_COUNT; ++i) {
        if (cache_pages[i].is_in_use && cache_pages[i].v_fd == v_fd && cache_pages[i].offset == page_offset) {
            return &cache_pages[i];
        }
    }
    return NULL;
}

static CachePage* load_page(int v_fd, off_t page_offset) {
    CachePage *page_slot = NULL;

    for (int i = 0; i < CACHE_PAGE_COUNT; ++i) {
        if (!cache_pages[i].is_in_use) {
            page_slot = &cache_pages[i];
            break;
        }
    }


    if (!page_slot) {
        page_slot = lru_tail;
        if (!page_slot) {
            return NULL;
        }


        
        lru_detach(page_slot);


        if (flush_page(page_slot) == -1) {
            perror("flush_page failed during eviction, aborting load");
            lru_add_to_head(page_slot); 
            return NULL; 
        }
    }


    int sys_fd = open_files[v_fd].sys_fd;
    ssize_t bytes_read = pread(sys_fd, page_slot->data, PAGE_SIZE, page_offset);

    if (bytes_read == -1) {
        perror("pread in load_page");

        page_slot->is_in_use = 0;
        return NULL;
    }

    if (bytes_read < PAGE_SIZE) {
        memset(page_slot->data + bytes_read, 0, PAGE_SIZE - bytes_read);
    }

    page_slot->v_fd = v_fd;
    page_slot->offset = page_offset;
    page_slot->is_dirty = 0;
    page_slot->is_in_use = 1;

    lru_add_to_head(page_slot);

    return page_slot;
}



// --- Реализация API ---

int vtpc_open(const char* path, int mode, int access) {
    if (cache_init() == -1) return -1;
    int v_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (!open_files[i].is_open) {
            v_fd = i;
            break;
        }
    }
    if (v_fd == -1) {
        errno = EMFILE;
        return -1;
    }

    int sys_fd = open(path, mode, access);
    if (sys_fd == -1) {
        perror("open failed in vtpc_open");
        return -1;
    }

    if (fcntl(sys_fd, F_NOCACHE, 1) == -1) {
        perror("fcntl(F_NOCACHE) failed");
        close(sys_fd);
        return -1;
    }


    open_files[v_fd].is_open = 1;
    open_files[v_fd].sys_fd = sys_fd;
    open_files[v_fd].seek_pos = 0;
    struct stat st;
    if (fstat(sys_fd, &st) == 0) {
        open_files[v_fd].file_size = st.st_size;
    } else {
        open_files[v_fd].file_size = 0;
    }
    return v_fd;
}

int vtpc_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].is_open) {
        errno = EBADF;
        return -1;
    }
    vtpc_fsync(fd);
    for (int i = 0; i < CACHE_PAGE_COUNT; ++i) {
        if (cache_pages[i].is_in_use && cache_pages[i].v_fd == fd) {
            lru_detach(&cache_pages[i]);
            cache_pages[i].is_in_use = 0;
            cache_pages[i].is_dirty = 0;
        }
    }
    ftruncate(open_files[fd].sys_fd, open_files[fd].file_size);
    close(open_files[fd].sys_fd);
    open_files[fd].is_open = 0;
    return 0;
}

off_t vtpc_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].is_open) {
        errno = EBADF;
        return -1;
    }
    if (whence != SEEK_SET) {
        errno = EINVAL;
        return -1;
    }
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    open_files[fd].seek_pos = offset;
    return offset;
}

ssize_t vtpc_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].is_open) {
        errno = EBADF;
        return -1;
    }
    off_t current_pos = open_files[fd].seek_pos;
    off_t file_size = open_files[fd].file_size;
    
    if (current_pos >= file_size) {
        return 0;
    }
    
    size_t bytes_to_read = count;
    if (current_pos + (off_t)bytes_to_read > file_size) {
        bytes_to_read = (size_t)(file_size - current_pos);
    }
    
    size_t total_bytes_read = 0;

    while (bytes_to_read > 0) {
        off_t page_offset = (current_pos / PAGE_SIZE) * PAGE_SIZE;
        off_t offset_in_page = current_pos % PAGE_SIZE;
        CachePage *page = find_page(fd, page_offset);
        if (!page) {
            page = load_page(fd, page_offset);
            if (!page) return total_bytes_read > 0 ? total_bytes_read : -1;
        }
        lru_detach(page);
        lru_add_to_head(page);
        size_t bytes_to_copy = bytes_to_read;
        if (offset_in_page + bytes_to_copy > PAGE_SIZE) {
            bytes_to_copy = PAGE_SIZE - offset_in_page;
        }
        memcpy((char*)buf + total_bytes_read, page->data + offset_in_page, bytes_to_copy);
        total_bytes_read += bytes_to_copy;
        bytes_to_read -= bytes_to_copy;
        current_pos += bytes_to_copy;
    }
    open_files[fd].seek_pos = current_pos;
    return total_bytes_read;
}

ssize_t vtpc_write(int fd, const void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].is_open) {
        errno = EBADF;
        return -1;
    }
    off_t current_pos = open_files[fd].seek_pos;
    size_t bytes_to_write = count;
    size_t total_bytes_written = 0;

    while (bytes_to_write > 0) {
        off_t page_offset = (current_pos / PAGE_SIZE) * PAGE_SIZE;
        off_t offset_in_page = current_pos % PAGE_SIZE;
        CachePage *page = find_page(fd, page_offset);
        if (!page) {
            page = load_page(fd, page_offset);
            if (!page) return total_bytes_written > 0 ? total_bytes_written : -1;
        }
        lru_detach(page);
        lru_add_to_head(page);
        size_t bytes_to_copy = bytes_to_write;
        if (offset_in_page + bytes_to_copy > PAGE_SIZE) {
            bytes_to_copy = PAGE_SIZE - offset_in_page;
        }
        memcpy(page->data + offset_in_page, (const char*)buf + total_bytes_written, bytes_to_copy);
        page->is_dirty = 1;
        total_bytes_written += bytes_to_copy;
        bytes_to_write -= bytes_to_copy;
        current_pos += bytes_to_copy;
    }
    open_files[fd].seek_pos = current_pos;
    
    if (current_pos > open_files[fd].file_size) {
        open_files[fd].file_size = current_pos;
    }
    
    return total_bytes_written;
}

int vtpc_fsync(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !open_files[fd].is_open) {
        errno = EBADF;
        return -1;
    }
    int result = 0;
    for (int i = 0; i < CACHE_PAGE_COUNT; ++i) {
        if (cache_pages[i].is_in_use && cache_pages[i].v_fd == fd && cache_pages[i].is_dirty) {
            if (flush_page(&cache_pages[i]) == -1) {
                result = -1; 
            }
        }
    }

    if (fsync(open_files[fd].sys_fd) == -1) {
        perror("fsync failed");
        return -1;
    }
    return result;
}
