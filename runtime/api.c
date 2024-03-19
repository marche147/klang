#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>

void __attribute__((noreturn)) fatal(const char* msg) {
    fprintf(stderr, "Fatal error: %s\n", msg);
    exit(1);
}

void do_printi(int64_t i) {
    printf("%ld\n", i);
}

void do_prints(const char* s) {
    printf("%s\n", s);
}

int64_t do_inputi(void) {
    char buf[1024];
    if(!fgets(buf, sizeof(buf), stdin)) {
        fatal("Failed to read input");
    }
    int64_t i;
    i = strtol(buf, NULL, 10);
    return i;
}

int64_t do_random(void) {
    int64_t r;
    int fd = open("/dev/urandom", O_RDONLY);
    if(!fd) {
        fatal("Failed to open /dev/urandom");
    }
    if(read(fd, &r, sizeof(r)) != sizeof(r)) {
        fatal("Failed to read from /dev/urandom");
    }
    close(fd);
    return r;
}

char* do_inputs() {
    char buffer[1024] = {0};
    if(!fgets(buffer, sizeof(buffer), stdin)) {
        fatal("Failed to read input");
    }
    return strdup(buffer);
}

struct array_t {
    int64_t* data;
    int64_t size;
};

struct array_t* do_array_new(int64_t size) {
    if(size > 100) {
        fatal("Array size too large");
    }
    
    struct array_t* arr = (struct array_t*)malloc(sizeof(struct array_t));
    if(!arr) {
        fatal("Failed to allocate array");
    }
    arr->data = (int64_t*)malloc(size * sizeof(int64_t));
    if(!arr->data) {
        fatal("Failed to allocate array data");
    }
    memset(arr->data, 0, size * sizeof(int64_t));
    arr->size = size;
    return arr;
}

int64_t do_array_load(struct array_t* arr, int64_t index) {
    if(!arr) {
        fatal("Array is null");
    }
    if(index < 0 || index >= arr->size) {
        fatal("Array index out of bounds");
    }
    return arr->data[index];
}

void do_array_store(struct array_t* arr, int64_t index, int64_t value) {
    if(!arr) {
        fatal("Array is null");
    }
    if(index < 0 || index >= arr->size) {
        fatal("Array index out of bounds");
    }
    arr->data[index] = value;
}