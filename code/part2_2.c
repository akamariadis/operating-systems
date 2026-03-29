#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s input output char\n", argv[0]);
        exit(1);
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) {
        char *args[] = {"./part1", argv[1], argv[2], argv[3], NULL};
        execv("./part1", args);
        perror("execv failed");
        exit(1);
    }
    else {
        wait(NULL);
        printf("[Parent] The child (through syscall: execv) has completed the execution.\n");
    }
    return 0;
}