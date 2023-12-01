
// SPDX-License-Identifier: MIT

#include "arrays.h"
#include "assertions.h"
#include "filesystem.h"
#include "filesystem/vfs_internal.h"
#include "filesystem/vfs_ramfs.h"
#include "gpio.h"
#include "housekeeping.h"
#include "log.h"
#include "malloc.h"
#include "memprotect.h"
#include "port/interrupt.h"
#include "process/process.h"
#include "rawprint.h"
#include "scheduler/scheduler.h"
#include "time.h"

#include <stdint.h>

#include <kbelf.h>

extern uint8_t const elf_rom[];
extern size_t const  elf_rom_len;

// Temporary kernel context until threading is implemented.
static isr_ctx_t kctx;

void debug_func(void *);
#define stack_size 8192
static uint8_t stack0[stack_size] ALIGNED_TO(STACK_ALIGNMENT);

static inline void check_ec(badge_err_t *ec) {
    if (ec && !badge_err_is_ok(ec)) {
        logkf(LOG_ERROR, "ELOC=%{d}, ECAUSE=%{d}", ec->location, ec->cause);
        sched_destroy_thread(NULL, sched_get_current_thread());
    }
}

// This is the entrypoint after the stack has been set up and the init functions
// have been run. Main is not allowed to return, so declare it noreturn.
void main() NORETURN;
void main() {
    badge_err_t err;

    // Install interrupt and trap handlers.
    interrupt_init(&kctx);

    // Set up memory protection.
    memprotect_init();

    // Set up timers and watchdogs.
    // This function must run within the first ~1s of power-on time and should be
    // called as early as possible.
    time_init();

    // Set up memory allocator.
    kernel_heap_init();

    // Set up multithreading.
    sched_init(&err);
    badge_err_assert_always(&err);
    hk_init();

    // A debug thread.
    sched_thread_t *const debug_thread_0 =
        sched_create_kernel_thread(&err, debug_func, NULL, stack0, stack_size, SCHED_PRIO_NORMAL);
    badge_err_assert_always(&err);
    sched_set_name(&err, debug_thread_0, "debug0");
    badge_err_assert_always(&err);
    sched_resume_thread(&err, debug_thread_0);
    badge_err_assert_always(&err);

    // Enter the scheduler
    sched_exec();
    __builtin_unreachable();
}

void debug_func(void *arg) {
    (void)arg;
    badge_err_t ec;

    // Create RAMFS.
    logk(LOG_DEBUG, "Creating RAMFS at /");
    fs_mount(&ec, FS_TYPE_RAMFS, NULL, "/", 0);
    check_ec(&ec);

    // Put the ROM in the RAMFS.
    file_t fd = fs_open(&ec, "/a.out", OFLAGS_CREATE | OFLAGS_WRITEONLY);
    check_ec(&ec);
    fs_write(&ec, fd, elf_rom, (long)elf_rom_len);
    check_ec(&ec);
    fs_close(&ec, fd);
    check_ec(&ec);

    // Start a process.
    pid_t pid = proc_create(&ec);
    check_ec(&ec);
    proc_start(&ec, pid, "/a.out");
    check_ec(&ec);
}
