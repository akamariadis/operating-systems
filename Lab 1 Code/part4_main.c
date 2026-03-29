#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/select.h>

#define MAX_WORKERS 20
#define MAX_CHUNKS 1000

typedef struct {
    int active;
    pid_t pid;
    int pipe_to_worker[2];
    int assigned_chunk;
} WorkerInfo;

typedef struct {
    long offset;
    long size;
    int status;
} ChunkInfo;

volatile sig_atomic_t status_requested = 0;

void sigusr1_handler(int sig) {
    status_requested = 1;
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void run_dispatcher(const char *filename, int pipe_fe_to_disp[2], int pipe_disp_to_fe[2], pid_t fe_pid) {
    close(pipe_fe_to_disp[1]);
    close(pipe_disp_to_fe[0]);

    set_nonblocking(pipe_fe_to_disp[0]);

    WorkerInfo workers[MAX_WORKERS];
    for (int i = 0; i < MAX_WORKERS; i++) workers[i].active = 0;

    ChunkInfo chunks[MAX_CHUNKS];
    int total_chunks = 0;
    int chunks_completed = 0;
    int total_found = 0;
    int search_in_progress = 0;
    char current_target = '\0';

    int pipe_work_to_disp[2];
    pipe(pipe_work_to_disp);
    set_nonblocking(pipe_work_to_disp[0]);

    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    char msg_buf[128];

    while (1) {
        int status;
        pid_t dead_pid = waitpid(-1, &status, WNOHANG);
        if (dead_pid > 0) {
            for (int i = 0; i < MAX_WORKERS; i++) {
                if (workers[i].active && workers[i].pid == dead_pid) {
                    workers[i].active = 0;
                    close(workers[i].pipe_to_worker[1]);

                    int c_id = workers[i].assigned_chunk;
                    if (c_id != -1 && chunks[c_id].status == 1) {
                        chunks[c_id].status = 0;
                    }
                    break;
                }
            }
        }

        if (status_requested) {
            status_requested = 0;
            char reply[128];
            if (!search_in_progress) {
                sprintf(reply, "STATUS: No search running. Found so far: %d\n", total_found);
            } else {
                int percent = (total_chunks == 0) ? 0 : (chunks_completed * 100) / total_chunks;
                sprintf(reply, "STATUS: Progress: %d%%. Chars found: %d\n", percent, total_found);
            }
            write(pipe_disp_to_fe[1], reply, strlen(reply) + 1);
            kill(fe_pid, SIGCONT);
        }

        if (read(pipe_fe_to_disp[0], msg_buf, sizeof(msg_buf)) > 0) {
            if (strncmp(msg_buf, "ADD", 3) == 0) {
                int added = 0;
                for (int i = 0; i < MAX_WORKERS; i++) {
                    if (!workers[i].active) {
                        pipe(workers[i].pipe_to_worker);
                        pid_t w_pid = fork();
                        if (w_pid == 0) {
                            close(pipe_fe_to_disp[0]); close(pipe_disp_to_fe[1]);
                            close(pipe_work_to_disp[0]); close(workers[i].pipe_to_worker[1]);

                            char id_str[10], in_str[10], out_str[10];
                            sprintf(id_str, "%d", i);
                            sprintf(in_str, "%d", workers[i].pipe_to_worker[0]);
                            sprintf(out_str, "%d", pipe_work_to_disp[1]);

                            char *args[] = {"./worker", id_str, in_str, out_str, (char *)filename, NULL};
                            execv("./worker", args);
                            exit(1);
                        } else {
                            workers[i].active = 1;
                            workers[i].pid = w_pid;
                            workers[i].assigned_chunk = -1;
                            close(workers[i].pipe_to_worker[0]);
                            added = 1;
                            break;
                        }
                    }
                }
                char *resp = added ? "Worker added.\n" : "Max workers reached.\n";
                write(pipe_disp_to_fe[1], resp, strlen(resp) + 1);
            }
            else if (strncmp(msg_buf, "SEARCH", 6) == 0) {
                current_target = msg_buf[7];
                struct stat st;
                stat(filename, &st);
                long file_size = st.st_size;
                long chunk_size = 1024;
                total_chunks = (file_size + chunk_size - 1) / chunk_size;
                for (int i = 0; i < total_chunks; i++) {
                    chunks[i].offset = i * chunk_size;
                    chunks[i].size = (i == total_chunks - 1) ? (file_size - i * chunk_size) : chunk_size;
                    chunks[i].status = 0;
                }
                chunks_completed = 0;
                total_found = 0;
                search_in_progress = 1;

                char resp[] = "Search started in background.\n";
                write(pipe_disp_to_fe[1], resp, strlen(resp) + 1);
            }
            else if (strncmp(msg_buf, "EXIT", 4) == 0) {
                for(int i=0; i<MAX_WORKERS; i++) if(workers[i].active) kill(workers[i].pid, SIGKILL);
                exit(0);
            }
        }
        if (search_in_progress) {
            for (int c = 0; c < total_chunks; c++) {
                if (chunks[c].status == 0) {
                    for (int w = 0; w < MAX_WORKERS; w++) {
                        if (workers[w].active && workers[w].assigned_chunk == -1) {
                            workers[w].assigned_chunk = c;
                            chunks[c].status = 1;

                            char task_msg[32];
                            sprintf(task_msg, "TSK %08ld %08ld %c", chunks[c].offset, chunks[c].size, current_target);
                            write(workers[w].pipe_to_worker[1], task_msg, 32);
                            break;
                        }
                    }
                }
            }
        }

        char res_msg[16];
        if (read(pipe_work_to_disp[0], res_msg, 16) == 16) {
            int w_id, found;
            sscanf(res_msg, "RES %d %d", &w_id, &found);

            total_found += found;
            chunks[workers[w_id].assigned_chunk].status = 2;
            workers[w_id].assigned_chunk = -1;
            chunks_completed++;

            if (search_in_progress && chunks_completed == total_chunks) {
                search_in_progress = 0;
                char final_msg[128];
                sprintf(final_msg, "\n[DISPATCHER] Search complete! Total '%c' found: %d\n> ", current_target, total_found);
                write(pipe_disp_to_fe[1], final_msg, strlen(final_msg) + 1);
            }
        }

        usleep(10000);
    }
}

void run_frontend(int pipe_fe_to_disp[2], int pipe_disp_to_fe[2], pid_t dispatcher_pid) {
    close(pipe_fe_to_disp[0]);
    close(pipe_disp_to_fe[1]);
    printf("=== ?~Zα?~Dανεμημένη ?~Qναζή?~Dη?~Cη Χα?~Aακ?~Dή?~A?~Iν (Front-End) ===\n");
    printf("?~Uν?~Dολέ?~B: add, status, search <char>, exit\n> ");
    fflush(stdout);

    fd_set readfds;
    char input[128];
    char reply[256];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(pipe_disp_to_fe[0], &readfds);

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
        select(MAX(STDIN_FILENO, pipe_disp_to_fe[0]) + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(pipe_disp_to_fe[0], &readfds)) {
            if (read(pipe_disp_to_fe[0], reply, sizeof(reply)) > 0) {
                printf("%s", reply);
                if (strncmp(reply, "\n[DISP", 6) != 0) printf("> ");
                fflush(stdout);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "add") == 0 || strncmp(input, "search ", 7) == 0 || strcmp(input, "exit") == 0) {
                if (strcmp(input, "add") == 0) write(pipe_fe_to_disp[1], "ADD", 4);
                else if (strcmp(input, "exit") == 0) write(pipe_fe_to_disp[1], "EXIT", 5);
                else {
                    char cmd[32];
                    sprintf(cmd, "SEARCH %c", input[7]);
                    write(pipe_fe_to_disp[1], cmd, strlen(cmd) + 1);
                }

                if (strcmp(input, "exit") == 0) break;
            }
            else if (strcmp(input, "status") == 0) {
                kill(dispatcher_pid, SIGUSR1);
            }
            else {
                printf("?~Fγν?~I?~C?~Dη εν?~Dολή.\n> ");
                fflush(stdout);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Χ?~Aή?~Cη: ./askisi4 <α?~A?~Gείο_ει?~C?~Lδο?~E>\n");
        exit(1);
    }

    int pipe_fe_to_disp[2], pipe_disp_to_fe[2];
    pipe(pipe_fe_to_disp);
    pipe(pipe_disp_to_fe);

    pid_t dispatcher_pid = fork();
    if (dispatcher_pid == 0) {
        run_dispatcher(argv[1], pipe_fe_to_disp, pipe_disp_to_fe, getppid());
        exit(0);
    }
    run_frontend(pipe_fe_to_disp, pipe_disp_to_fe, dispatcher_pid);

    waitpid(dispatcher_pid, NULL, 0);
    return 0;
}