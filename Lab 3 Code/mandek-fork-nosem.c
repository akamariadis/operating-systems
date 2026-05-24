#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

int y_chars = 50;
int x_chars = 90;

double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;

double xstep;
double ystep;

void compute_mandel_line(int line, int color_val[])
{       
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

void output_mandel_line(int fd, int color_val[])
{       
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
void *create_shared_memory_area(unsigned int numbytes)
{
        int pages;
        void *addr;
        long page_size;

        if (numbytes == 0) {
                fprintf(stderr, "%s: internal error: called for numbytes == 0\n", __func__);
                exit(1);
        }

        page_size = sysconf(_SC_PAGE_SIZE);
        pages = (numbytes - 1) / page_size + 1;

        addr = mmap(NULL,
                    pages * page_size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS,
                    -1,
                    0);

        if (addr == MAP_FAILED) {
                perror("create_shared_memory_area: mmap failed");
                exit(1);
        }

        return addr;
}

void destroy_shared_memory_area(void *addr, unsigned int numbytes)
{
        int pages;
        long page_size;

        if (numbytes == 0) {
                fprintf(stderr, "%s: internal error: called for numbytes == 0\n", __func__);
                exit(1);
        }

        page_size = sysconf(_SC_PAGE_SIZE);
        pages = (numbytes - 1) / page_size + 1;

        if (munmap(addr, pages * page_size) == -1) {
                perror("destroy_shared_memory_area: munmap failed");
                exit(1);
        }
}
int main(int argc, char **argv)
{
        int line;
        int i;
        int nprocs;
        pid_t p;
        int *shared_output;

        if (argc != 2) {
                fprintf(stderr, "Usage: %s NPROCS\n", argv[0]);
                exit(1);
        }

        nprocs = atoi(argv[1]);
        if (nprocs <= 0) {
                fprintf(stderr, "NPROCS must be a positive integer\n");
                exit(1);
        }
xstep = (xmax - xmin) / x_chars;
        ystep = (ymax - ymin) / y_chars;

        shared_output = create_shared_memory_area(y_chars * x_chars * sizeof(int));

        for (i = 0; i < nprocs; i++) {
                p = fork();

                if (p < 0) {
                        perror("fork");
                        exit(1);
                }

                if (p == 0) {
                        int my_id = i;

                        for (line = my_id; line < y_chars; line += nprocs) {
                                int *row;

                                row = shared_output + line * x_chars;
                                compute_mandel_line(line, row);
                        }

                        exit(0);
                }
        }

        for (i = 0; i < nprocs; i++) {
                if (wait(NULL) == -1) {
                        perror("wait");
                        exit(1);
                }
        }

        for (line = 0; line < y_chars; line++) {
                int *row;

                row = shared_output + line * x_chars;
                output_mandel_line(1, row);
        }

        reset_xterm_color(1);

        destroy_shared_memory_area(shared_output, y_chars * x_chars * sizeof(int));

        return 0;
}
