
// SPDX-License-Identifier: MIT

#include "assertions.h"
#include "badge_strings.h"
#include "filesystem.h"
#include "malloc.h"
#include "process/internal.h"
#include "process/process.h"
#include "process/types.h"

#include <kbelf.h>



// Measure the length of `str`.
size_t kbelfq_strlen(char const *str) {
    return cstr_length(str);
}

// Copy string from `src` to `dst`.
void kbelfq_strcpy(char *dst, char const *src) {
    cstr_copy(dst, SIZE_MAX, src);
}

// Find last occurrance of `c` in `str`.
char const *kbelfq_strrchr(char const *str, char c) {
    ptrdiff_t off = cstr_last_index(str, c);
    return off == -1 ? NULL : off + str;
}

// Compare string `a` to `b`.
bool kbelfq_streq(char const *a, char const *b) {
    return cstr_equals(a, b);
}

// Copy memory from `src` to `dst`.
void kbelfq_memcpy(void *dst, void const *src, size_t nmemb) {
    mem_copy(dst, src, nmemb);
}

// Fill memory `dst` with `c`.
void kbelfq_memset(void *dst, uint8_t c, size_t nmemb) {
    mem_set(dst, c, nmemb);
}

// Compare memory `a` to `b`.
bool kbelfq_memeq(void const *a, void const *b, size_t nmemb) {
    return mem_equals(a, b, nmemb);
}



// Memory allocator function to use for allocating metadata.
// User-defined.
void *kbelfx_malloc(size_t len) {
    return malloc(len);
}

// Memory allocator function to use for allocating metadata.
// User-defined.
void *kbelfx_realloc(void *mem, size_t len) {
    return realloc(mem, len);
}

// Memory allocator function to use for allocating metadata.
// User-defined.
void kbelfx_free(void *mem) {
    free(mem);
}


// Memory allocator function to use for loading program segments.
// Takes a segment with requested address and permissions and returns a segment with physical and virtual address
// information. Returns success status. User-defined.
bool kbelfx_seg_alloc(kbelf_inst inst, size_t segs_len, kbelf_segment *segs) {
    process_t *proc = proc_get(kbelf_inst_getpid(inst));
    assert_dev_keep(proc != NULL);

    size_t min_addr  = SIZE_MAX;
    size_t max_addr  = 0;
    size_t min_align = 16;

    for (size_t i = 0; i < segs_len; i++) {
        size_t start = segs[i].vaddr_req;
        if (start < min_addr)
            min_addr = start;
        size_t end = segs[i].vaddr_req + segs[i].size;
        if (end > max_addr)
            max_addr = end;
        logkf(LOG_DEBUG, "Segment %{size;d}: %{size;x} - %{size;x}", i, start, end);
    }

    size_t vaddr_real = proc_map_raw(NULL, proc, min_addr, max_addr - min_addr, min_align);
    if (!vaddr_real)
        return false;

    for (size_t i = 0; i < segs_len; i++) {
        segs[i].vaddr_real   = segs[i].vaddr_req - min_addr + vaddr_real;
        segs[i].paddr        = segs[i].vaddr_real;
        segs[i].laddr        = segs[i].vaddr_real;
        segs[i].alloc_cookie = NULL;
        logkf(LOG_DEBUG, "Segment %{size;x} mapped to %{size;x}", i, segs[i].vaddr_real);
    }
    segs[0].alloc_cookie = (void *)vaddr_real;

    return true;
}

// Memory allocator function to use for loading program segments.
// Takes a previously allocated segment and unloads it.
// User-defined.
void kbelfx_seg_free(kbelf_inst inst, size_t segs_len, kbelf_segment *segs) {
    (void)segs_len;
    (void)segs;
    process_t *proc = proc_get(kbelf_inst_getpid(inst));
    assert_dev_keep(proc != NULL);
    proc_unmap_raw(NULL, proc, (size_t)segs[0].alloc_cookie);
}


// Open a binary file for reading.
// User-defined.
void *kbelfx_open(char const *path) {
    file_t fd = fs_open(NULL, path, OFLAGS_READONLY);
    if (fd == -1)
        return NULL;
    else
        return (void *)(fd + 1);
}

// Close a file.
// User-defined.
void kbelfx_close(void *fd) {
    fs_close(NULL, (int)fd - 1);
}

// Reads a single byte from a file.
// Returns byte on success, -1 on error.
// User-defined.
int kbelfx_getc(void *fd) {
    char      buf;
    fileoff_t len = fs_read(NULL, (int)fd - 1, &buf, 1);
    return len > 0 ? buf : -1;
}

// Reads a number of bytes from a file.
// Returns the number of bytes read, or less than that on error.
// User-defined.
int kbelfx_read(void *fd, void *buf, int buf_len) {
    return fs_read(NULL, (int)fd - 1, buf, buf_len);
}

// Sets the absolute offset in the file.
// Returns 0 on success, -1 on error.
// User-defined.
int kbelfx_seek(void *fd, long pos) {
    fileoff_t q = fs_seek(NULL, (int)fd - 1, pos, SEEK_ABS);
    return pos == q ? 0 : -1;
}


// Find and open a dynamic library file.
// Returns non-null on success, NULL on error.
// User-defined.
kbelf_file kbelfx_find_lib(char const *needed) {
    (void)needed;
    return NULL;
}



// A built-in library function test.
static void exit_impl(int code) {
    (void)code;
    __builtin_trap();
}

kbelf_builtin_sym const my_syms[] = {{"exit", (size_t)&exit_impl, (size_t)&exit_impl, 0}};

kbelf_builtin_lib const my_libs[] = {{"userlandlib.so", 1, my_syms, 0}};

// Number of built-in libraries.
// Optional user-defined.
size_t                   kbelfx_builtin_libs_len = 1;
// Array of built-in libraries.
// Optional user-defined.
kbelf_builtin_lib const *kbelfx_builtin_libs     = my_libs;
