#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

extern int64_t K_main(void);

static void init_io(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

int main(int argc, char* argv[], char* envp[]) {
    init_io();
    alarm(20);
    return K_main();
}