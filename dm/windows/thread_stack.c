// SDK 给的客体栈不能交给 Windows CRT 展开；通过显式栈桥返回宿主后退出线程。
// Windows CRT must not unwind SDK-owned stacks; return through an explicit stack bridge before exiting the thread.
#include "dm_windows.h"
typedef struct { uint64_t registers[24]; } saved_stack;
static _Thread_local saved_stack *active;
extern void dm_windows_guest_enter(uint64_t entry, uint64_t argument, uint64_t sp, saved_stack *saved, uint64_t limit);
extern void dm_windows_guest_leave(saved_stack *saved) __attribute__((noreturn));
void dm_thread_enter(uint64_t entry, uint64_t argument, uint64_t sp) {
    MEMORY_BASIC_INFORMATION memory;
    if (active || sp & 15 || !VirtualQuery((void *)(uintptr_t)(sp - 1), &memory, sizeof memory) || memory.State != MEM_COMMIT) {
        fprintf(stderr, "[dm] invalid or nested Windows guest stack\n"); abort();
    }
    saved_stack saved; active = &saved;
    dm_windows_guest_enter(entry, argument, sp, &saved, (uintptr_t)memory.BaseAddress);
    active = NULL;
    pthread_exit(NULL);
}
void dm_windows_thread_exit(void) {
    if (active) dm_windows_guest_leave(active);
    pthread_exit(NULL);
    __builtin_unreachable();
}
