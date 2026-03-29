#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

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

    close(fd_in);
    close(fd_out);

    return 0;
}