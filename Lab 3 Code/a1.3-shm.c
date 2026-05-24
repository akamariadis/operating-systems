#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define BUF_SIZE 4096

struct shared_data {
    sem_t sem;
    int counter;
};

static struct shared_data *shared = NULL;

void sigint_handler(int signo)
{
    (void) signo;

    if (shared != NULL) {
        printf("\nSIGINT: current shared counter = %d\n", shared->counter);
        fflush(stdout);
    }
}

void count_part(const char *filename, char target, off_t start, off_t end)
{
    int fd;
    char buf[BUF_SIZE];
    off_t pos;
    ssize_t nread;
    size_t to_read;
    int i;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open child");
        exit(1);
    }

    pos = start;

    while (pos < end) {
        if (end - pos < BUF_SIZE)
            to_read = end - pos;
        else
            to_read = BUF_SIZE;

        nread = pread(fd, buf, to_read, pos);

        if (nread < 0) {
            if (errno == EINTR)
                continue;
perror("pread");
            close(fd);
            exit(1);
        }

        if (nread == 0)
            break;

        for (i = 0; i < nread; i++) {
            if (buf[i] == target) {
                sem_wait(&shared->sem);
                shared->counter++;
                sem_post(&shared->sem);
            }
        }

        pos += nread;
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    const char *input_file;
    const char *output_file;
    char target;
    int nprocs;
    int fd;
    FILE *fpw;
    struct stat st;
    off_t file_size;
    int i;
    pid_t pid;
    int status;

    if (argc != 5) {
        fprintf(stderr, "Usage: %s input_file output_file character nprocs\n", argv[0]);
        exit(1);
    }

    input_file = argv[1];
    output_file = argv[2];
    target = argv[3][0];
    nprocs = atoi(argv[4]);

    if (nprocs <= 0) {
        fprintf(stderr, "nprocs must be positive\n");
        exit(1);
    }

    fd = open(input_file, O_RDONLY);
    if (fd < 0) {
        perror("open input");
        exit(1);
    }

    if (fstat(fd, &st) < 0) {
perror("fstat");
        close(fd);
        exit(1);
    }

    file_size = st.st_size;
    close(fd);

    shared = mmap(NULL,
                  sizeof(struct shared_data),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS,
                  -1,
                  0);

    if (shared == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    shared->counter = 0;

    if (sem_init(&shared->sem, 1, 1) < 0) {
        perror("sem_init");
        munmap(shared, sizeof(struct shared_data));
        exit(1);
    }

    signal(SIGINT, sigint_handler);

    for (i = 0; i < nprocs; i++) {
        pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            off_t start;
            off_t end;

            signal(SIGINT, SIG_IGN);

            start = (file_size * i) / nprocs;
            end = (file_size * (i + 1)) / nprocs;

            count_part(input_file, target, start, end);

            exit(0);
        }
    }

    for (i = 0; i < nprocs; i++) {
        while (wait(&status) < 0) {
            if (errno == EINTR)
                continue;

            perror("wait");
exit(1);
        }
    }

    fpw = fopen(output_file, "w");
    if (fpw == NULL) {
        perror("fopen output");
        sem_destroy(&shared->sem);
        munmap(shared, sizeof(struct shared_data));
        exit(1);
    }

    fprintf(fpw,
            "The character '%c' appears %d times in file %s.\n",
            target,
            shared->counter,
            input_file);

    fclose(fpw);

    printf("The character '%c' appears %d times in file %s.\n",
           target,
           shared->counter,
           input_file);

    sem_destroy(&shared->sem);
    munmap(shared, sizeof(struct shared_data));

    return 0;
}
