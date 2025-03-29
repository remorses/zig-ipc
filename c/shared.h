#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <stdbool.h>

#define NUMBERS_MAX 31
// On macOS, shared memory objects must be in /tmp
#define SHM_NAME "/tmp/ipc-test"

typedef struct {
    uint32_t length;
    uint32_t numbers[NUMBERS_MAX];
} Data;

typedef struct {
    bool busy;
    Data data;
} Value;

#endif // SHARED_H 