#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

int y_chars = 50;
int x_chars = 90;
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;
double xstep;
double ystep;
int NTHREADS;
sem_t *sems;

typedef struct {
    int tid;
} thread_arg_t;

void compute_mandel_line(int line, int color_val[]) {
    double x, y;
    int n;
    int val;
    y = ymax - ystep * line;
    for (x = xmin, n = 0; n < x_chars; x += xstep, n++) {
        val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
        if (val > 255)
            val = 255;
        val = xterm_color(val);
        color_val[n] = val;
    }
}

void output_mandel_line(int fd, int color_val[]) {
    int i;
    char point = '@';
    char newline = '\n';
    for (i = 0; i < x_chars; i++) {
        set_xterm_color(fd, color_val[i]);
        if (write(fd, &point, 1) != 1) {
            perror("output_mandel_line: write point");
            exit(1);
        }
    }
    if (write(fd, &newline, 1) != 1) {
        perror("output_mandel_line: write newline");
        exit(1);
    }
}

void *thread_func(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    int tid = targ->tid;
    for (int line = tid; line < y_chars; line += NTHREADS) {
        int color_val[x_chars];
        compute_mandel_line(line, color_val);
        sem_wait(&sems[tid]);
        output_mandel_line(STDOUT_FILENO, color_val);
        sem_post(&sems[(tid + 1) % NTHREADS]);
    }
    return NULL;
}

int main(int argc, char **argv) {
    pthread_t *threads;
    thread_arg_t *args;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NTHREADS\n", argv[0]);
        exit(1);
    }
    NTHREADS = atoi(argv[1]);
    if (NTHREADS <= 0) {
        fprintf(stderr, "NTHREADS must be positive\n");
        exit(1);
    }
    xstep = (xmax - xmin) / x_chars;
    ystep = (ymax - ymin) / y_chars;
    threads = malloc(NTHREADS * sizeof(pthread_t));
    args = malloc(NTHREADS * sizeof(thread_arg_t));
    sems = malloc(NTHREADS * sizeof(sem_t));
    if (threads == NULL || args == NULL || sems == NULL) {
        perror("malloc");
        exit(1);
    }
    for (int i = 0; i < NTHREADS; i++) {
        sem_init(&sems[i], 0, i == 0 ? 1 : 0);
    }
    for (int i = 0; i < NTHREADS; i++) {
        args[i].tid = i;

        if (pthread_create(&threads[i], NULL, thread_func, &args[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }
    for (int i = 0; i < NTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    reset_xterm_color(STDOUT_FILENO);
    for (int i = 0; i < NTHREADS; i++) {
        sem_destroy(&sems[i]);
    }
    free(sems);
    free(args);
    free(threads);
    return 0;
}
