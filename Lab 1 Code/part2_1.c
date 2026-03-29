#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        write(2, "Usage: program input output char\n", 33);
        exit(1);
    }
    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in < 0) {
        perror("open input");
        exit(1);
    }

    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("open output");
        exit(1);
    }
    int x = 10;
    printf("[Parent] Variable x initialized to: %d\n", x);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) {
        printf("[Child] Hello Worlds! My PID is %d. My parent's PID is %d.\n", getpid(), getppid());
        x = 20;
        printf("[Child] I changed x to: %d\n", x);
        char buffer[1024];
        ssize_t bytes;
        char target = argv[3][0];
        int count = 0;
        while ((bytes = read(fd_in, buffer, sizeof(buffer))) > 0) {
            for (int i = 0; i < bytes; i++) {
                if (buffer[i] == target) {
                    count++;
                }
            }
        }
        if (bytes < 0) {
            perror("read");
            exit(1);
        }
        char result[50];
        int len = sprintf(result, "Count: %d\n", count);
        write(fd_out, result, len);
        exit(0);
    }
    else {
        printf("[Parent] I created the child whose PID is: %d\n", pid);
        x = 30;
        printf("[Parent] I changed x to: %d\n", x);
        wait(NULL);
        printf("[Parent] Child, terminated.\n");
        close(fd_in);
        close(fd_out);
    }
    return 0;
}