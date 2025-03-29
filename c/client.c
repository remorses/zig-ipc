#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "shared.h"

#define N_FILL 3
#define N_APPEND 10
#define MAX 100

void fill_data(Value* shared, uint32_t* numbers, size_t count) {
    bool expected = false;
    while (!__atomic_compare_exchange_n(&shared->busy, &expected, true, false, 
                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        expected = false;
        usleep(1000);
    }

    shared->data.length = count;
    memcpy(shared->data.numbers, numbers, count * sizeof(uint32_t));

    __atomic_store_n(&shared->busy, false, __ATOMIC_SEQ_CST);
}

void append_number(Value* shared, uint32_t value) {
    bool expected = false;
    while (!__atomic_compare_exchange_n(&shared->busy, &expected, true, false, 
                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        expected = false;
        usleep(1000);
    }

    if (shared->data.length < NUMBERS_MAX) {
        shared->data.numbers[shared->data.length] = value;
        shared->data.length++;
    }

    __atomic_store_n(&shared->busy, false, __ATOMIC_SEQ_CST);
}

int main() {
    // Open the shared memory object
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        printf("shm_open failed: %s\n", strerror(errno));
        printf("Make sure the server is running first!\n");
        return 1;
    }

    // Map the shared memory object
    Value* shared = mmap(NULL, sizeof(Value), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    // Initialize random number generator
    srand(time(NULL));

    // Fill initial numbers
    uint32_t numbers[N_FILL];
    printf("Filled(%d): ", N_FILL);
    for (int i = 0; i < N_FILL; i++) {
        numbers[i] = rand() % MAX;
        printf("%u ", numbers[i]);
    }
    printf("\n");
    fill_data(shared, numbers, N_FILL);

    // Append additional numbers
    for (int i = 1; i <= N_APPEND; i++) {
        uint32_t n = rand() % MAX;
        append_number(shared, n);
        printf("Appended(%d): %u\n", i, n);
        usleep(100000); // Small delay between appends
    }

    // Cleanup
    munmap(shared, sizeof(Value));
    close(fd);

    return 0;
} 