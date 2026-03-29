#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "[WORKER] Invalid arguments.\n");
        exit(1);
    }

    int worker_id = atoi(argv[1]);
    int fd_in = atoi(argv[2]);
    int fd_out = atoi(argv[3]);
    const char *filename = argv[4];

    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) exit(1);

    char task_msg[64];
    while (read(fd_in, task_msg, 32) > 0) {
        long offset, size;
        char target;

        sscanf(task_msg, "TSK %08ld %08ld %c", &offset, &size, &target);

        char buffer[1024];
        int local_count = 0;
        long total_read = 0;

        lseek(file_fd, offset, SEEK_SET);

        while (total_read < size) {
            long to_read = (size - total_read > sizeof(buffer)) ? sizeof(buffer) : (size - total_read);
            ssize_t bytes = read(file_fd, buffer, to_read);
            if (bytes <= 0) break;

            for (int i = 0; i < bytes; i++) {
                if (buffer[i] == target) local_count++;
            }
            total_read += bytes;
        }

        usleep(500000);

        char res_msg[16];
        sprintf(res_msg, "RES %05d %05d", worker_id, local_count);
        write(fd_out, res_msg, 16);
    }

    close(file_fd);
    close(fd_in);
    close(fd_out);
    return 0;
}