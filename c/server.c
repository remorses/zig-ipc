#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "shared.h"

static volatile int early_exit = 0;

void handle_signal(int sig) {
    if (sig == SIGINT) {
        early_exit = 1;
    }
}

int compare_uint32(const void* a, const void* b) {
    return (*(uint32_t*)a - *(uint32_t*)b);
}

int main() {
    signal(SIGINT, handle_signal);

    // First try to unlink any existing shared memory
    shm_unlink(SHM_NAME);

    // Create the shared memory object
    printf("O_CREAT | O_RDWR: %o\n", O_CREAT | O_RDWR);
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        printf("shm_open failed: %s\n", strerror(errno));
        return 1;
    }

    // Set the size of the shared memory object
    if (ftruncate(fd, sizeof(Value)) == -1) {
        printf("ftruncate failed: %s\n", strerror(errno));
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    // Map the shared memory object into memory
    Value* shared = mmap(NULL, sizeof(Value), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    // Initialize the shared memory
    shared->busy = false;
    shared->data.length = 0;

    uint32_t last_length = shared->data.length;
    printf("Server started. Press Ctrl+C to exit.\n");

    while (!early_exit) {
        // Try to acquire lock
        bool expected = false;
        while (!__atomic_compare_exchange_n(&shared->busy, &expected, true, false, 
                                          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            expected = false;
            usleep(1000); // Small sleep to prevent busy waiting
        }

        if (last_length != shared->data.length) {
            printf("Sorting(%u): ", shared->data.length);
            for (uint32_t i = 0; i < shared->data.length; i++) {
                printf("%u ", shared->data.numbers[i]);
            }
            printf("\n");

            qsort(shared->data.numbers, shared->data.length, sizeof(uint32_t), compare_uint32);
            usleep(500000); // 500ms sleep to simulate slow sort

            printf("Sorted(%u): ", shared->data.length);
            for (uint32_t i = 0; i < shared->data.length; i++) {
                printf("%u ", shared->data.numbers[i]);
            }
            printf("\n");

            last_length = shared->data.length;
        }

        // Release lock
        __atomic_store_n(&shared->busy, false, __ATOMIC_SEQ_CST);
        usleep(1000); // Small sleep to prevent busy loop
    }

    // Cleanup
    munmap(shared, sizeof(Value));
    close(fd);
    shm_unlink(SHM_NAME);
    printf("\nServer shutdown complete\n");

    return 0;
} 