#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define P 3

volatile sig_atomic_t active_children = 0;

void safe_print_int(int num) {
    char buf[32];
    int i = 0;
    if (num == 0) {
        write(STDOUT_FILENO, "0", 1);
        return;
    }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        write(STDOUT_FILENO, &buf[j], 1);
    }
}

void sigint_handler(int sig) {
    write(STDOUT_FILENO, "\n[SIGNAL] Control+C caught! Active search processes: ", 53);
    safe_print_int(active_children);
    write(STDOUT_FILENO, "\n", 1);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        write(STDERR_FILENO, "Usage: ./askisi3 input_file output_file char\n", 45);
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    int fd_temp = open(argv[1], O_RDONLY);
    if (fd_temp < 0) {
        perror("open input");
        exit(1);
    }
    off_t file_size = lseek(fd_temp, 0, SEEK_END);
    close(fd_temp);

    off_t chunk_size = file_size / P;

    printf("[PARENT] Starting creation of %d children...\n", P);
    printf("[PARENT] File size is %ld bytes. You have 5 seconds to press Ctrl+C!\n", (long)file_size);

    for (int i = 0; i < P; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
        else if (pid == 0) {
            signal(SIGINT, SIG_IGN);
            close(pipefd[0]);

            sleep(5);

            int fd_in = open(argv[1], O_RDONLY);

            off_t start = i * chunk_size;
            off_t end = (i == P - 1) ? file_size : start + chunk_size;

            lseek(fd_in, start, SEEK_SET);

            char buffer[1024];
            ssize_t bytes;
            char target = argv[3][0];
            int local_count = 0;
            off_t bytes_to_read = end - start;
            off_t total_read = 0;

            while (total_read < bytes_to_read) {
                size_t to_read = sizeof(buffer);
                if (bytes_to_read - total_read < sizeof(buffer)) {
                    to_read = bytes_to_read - total_read;
                }

                bytes = read(fd_in, buffer, to_read);
                if (bytes <= 0) break;

                for (int j = 0; j < bytes; j++) {
                    if (buffer[j] == target) local_count++;
                }
                total_read += bytes;
            }
            close(fd_in);

            write(pipefd[1], &local_count, sizeof(int));
            close(pipefd[1]);
            exit(0);
        }
        else {
            active_children++;
        }
    }
    close(pipefd[1]);

    int total_count = 0;
    int partial_count;

    for (int i = 0; i < P; i++) {
        if (read(pipefd[0], &partial_count, sizeof(int)) > 0) {
            total_count += partial_count;
        }
    }
    close(pipefd[0]);

    pid_t wpid;
    int status;
    while ((wpid = wait(&status)) > 0) {
        active_children--;
    }

    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out >= 0) {
        char result_msg[50];
        int len = sprintf(result_msg, "Total Count: %d\n", total_count);
        write(fd_out, result_msg, len);
        close(fd_out);
    }

    printf("[PARENT] Search completed. Found %d occurrences.\n", total_count);

    return 0;
}