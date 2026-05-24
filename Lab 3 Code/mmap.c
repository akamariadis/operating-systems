#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <sys/wait.h>

#include "help.h"

#define RED     "\033[31m"
#define RESET   "\033[0m"


char *heap_private_buf;
char *heap_shared_buf;

char *file_shared_buf;

uint64_t buffer_size;


/*
 * Child process' entry point.
 */

void child(void) {
        uint64_t pa;

//DONE

        /*
         * Step 7 - Child
         */

        if (0 != raise(SIGSTOP))
                die("raise(SIGSTOP)");
        printf("\n[CHILD] Virtual Memory Map:\n");
        show_maps();

//DONE

        /*
         * Step 8 - Child
         */
        if (0 != raise(SIGSTOP))
                die("raise(SIGSTOP)");
        pa = get_physical_address((uintptr_t)heap_private_buf);
        printf("[CHILD] Physical address of heap_private_buf: 0x%lx\n", pa);

//DONE
/*
         * Step 9 - Child
         */

        if (0 != raise(SIGSTOP))
                die("raise(SIGSTOP)");
        heap_private_buf[0] = 'C';
        pa = get_physical_address((uintptr_t)heap_private_buf);
        printf("[CHILD] Physical address of heap_private_buf AFTER write: 0x%lx\n", pa);

//DONE

        /*
         * Step 10 - Child
         */

        if (0 != raise(SIGSTOP))
                die("raise(SIGSTOP)");
        heap_shared_buf[0] = 'S';
        pa = get_physical_address((uintptr_t)heap_shared_buf);
        printf("[CHILD] Physical address of heap_shared_buf AFTER write: 0x%lx\n", pa);

//DONE

        /*
         * Step 11 - Child
         */

        if (0 != raise(SIGSTOP))
                die("raise(SIGSTOP)");
        if (mprotect(heap_shared_buf, buffer_size, PROT_READ) == -1) die("mprotect");
        printf("\n[CHILD] Verifying maps (Child should now have r-- permissions on shared buffer):\n");
        show_maps();

//DONE

        /*
         * Step 12 - Child
         */

        munmap(heap_private_buf, buffer_size);
        munmap(heap_shared_buf, buffer_size);
}

/*
 * Parent process' entry point.
 */

void parent(pid_t child_pid) {
        uint64_t pa;
        int status;
        /* Wait for the child to raise its first SIGSTOP. */
        if (-1 == waitpid(child_pid, &status, WUNTRACED))
                die("waitpid");

//DONE
/*
         * Step 7: Print parent's and child's maps. What do you see?
         * Step 7 - Parent
         */

        printf(RED "\nStep 7: Print parent's and child's map.\n" RESET);
        press_enter();
        printf("\n[PARENT] Virtual Memory Map:\n");
        show_maps();
        if (-1 == kill(child_pid, SIGCONT))
                die("kill");
        if (-1 == waitpid(child_pid, &status, WUNTRACED))
                die("waitpid");

//DONE

        /*
         * Step 8: Get the physical memory address for heap_private_buf.
         * Step 8 - Parent
         */

        printf(RED "\nStep 8: Find the physical address of the private heap "
                "buffer (main) for both the parent and the child.\n" RESET);
        press_enter();
        pa = get_physical_address((uintptr_t)heap_private_buf);
        printf("[PARENT] Physical address of heap_private_buf: 0x%lx\n", pa);

        if (-1 == kill(child_pid, SIGCONT))
                die("kill");
        if (-1 == waitpid(child_pid, &status, WUNTRACED))
                die("waitpid");

//DONE

        /*
         * Step 9: Write to heap_private_buf. What happened?
         * Step 9 - Parent
         */

        printf(RED "\nStep 9: Write to the private buffer from the child and "
                "repeat step 8. What happened?\n" RESET);
        press_enter();
        pa = get_physical_address((uintptr_t)heap_private_buf);
        printf("[PARENT] Physical address of heap_private_buf: 0x%lx\n", pa);
        if (-1 == kill(child_pid, SIGCONT))
                die("kill");
        if (-1 == waitpid(child_pid, &status, WUNTRACED))
                die("waitpid");

//DONE

        /*
         * Step 10: Get the physical memory address for heap_shared_buf.
         * Step 10 - Parent
         */
printf(RED "\nStep 10: Write to the shared heap buffer (main) from "
                "child and get the physical address for both the parent and "
                "the child. What happened?\n" RESET);
        press_enter();
        pa = get_physical_address((uintptr_t)heap_shared_buf);
        printf("[PARENT] Physical address of heap_shared_buf: 0x%lx\n", pa);

        if (-1 == kill(child_pid, SIGCONT))
                die("kill");
        if (-1 == waitpid(child_pid, &status, WUNTRACED))
                die("waitpid");

//DONE

        /*
         * Step 11: Disable writing on the shared buffer for the child
         * (hint: mprotect(2)).
         * Step 11 - Parent
         */

        printf(RED "\nStep 11: Disable writing on the shared buffer for the "
                "child. Verify through the maps for the parent and the "
                "child.\n" RESET);
        press_enter();
        printf("\n[PARENT] Verifying maps (Parent should still have rw- permissions):\n");
        show_maps();
        if (-1 == kill(child_pid, SIGCONT))
                die("kill");
        if (-1 == waitpid(child_pid, &status, 0))
                die("waitpid");

//DONE

        /*
         * Step 12: Free all buffers for parent and child.
         * Step 12 - Parent
         */
        munmap(heap_private_buf, buffer_size);
        munmap(heap_shared_buf, buffer_size);
}

int main(void) {
        pid_t mypid, p;
        int fd = -1;
        uint64_t pa;
        mypid = getpid();
        buffer_size = 1 * get_page_size();
//DONE

        /*
         * Step 1: Print the virtual address space layout of this process.
         */

        printf(RED "\nStep 1: Print the virtual address space map of this "
                "process [%d].\n" RESET, mypid);
        press_enter();
        show_maps();

//DONE

        /*
         * Step 2: Use mmap to allocate a buffer of 1 page and print the map
         * again. Store buffer in heap_private_buf.
         */

        printf(RED "\nStep 2: Use mmap(2) to allocate a private buffer of "
                "size equal to 1 page and print the VM map again.\n" RESET);
        press_enter();
        heap_private_buf = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (heap_private_buf == MAP_FAILED) {
                die("mmap private buffer failed");
        }
        show_maps();

//DONE

        /*
         * Step 3: Find the physical address of the first page of your buffer
         * in main memory. What do you see?
         */

        printf(RED "\nStep 3: Find and print the physical address of the "
                "buffer in main memory. What do you see?\n" RESET);
        press_enter();
        pa = get_physical_address((uintptr_t)heap_private_buf);
        printf("Physical address of heap_private_buf: 0x%lx\n", pa);

//DONE

        /*
         * Step 4: Write zeros to the buffer and repeat Step 3.
         */
printf(RED "\nStep 4: Initialize your buffer with zeros and repeat "
                "Step 3. What happened?\n" RESET);
        press_enter();
        memset(heap_private_buf, 0, buffer_size);
        pa = get_physical_address((uintptr_t)heap_private_buf);
        printf("Physical address of heap_private_buf after memset: 0x%lx\n", pa);

//DONE

        /*
         * Step 5: Use mmap(2) to map file.txt (memory-mapped files) and print
         * its content. Use file_shared_buf.
         */

        printf(RED "\nStep 5: Use mmap(2) to read and print file.txt. Print "
                "the new mapping information that has been created.\n" RESET);
        press_enter();
        fd = open("file.txt", O_RDONLY);
        if (fd == -1)
                die("open file.txt");
        struct stat st;
        if (fstat(fd, &st) == -1)
                die("fstat");
        file_shared_buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (file_shared_buf == MAP_FAILED)
                die("mmap file");
        printf("--- File Content ---\n");
        write(STDOUT_FILENO, file_shared_buf, st.st_size);
        printf("\n--------------------\n");
        show_maps();

//DONE

        /*
         * Step 6: Use mmap(2) to allocate a shared buffer of 1 page. Use
         * heap_shared_buf.
         */

        printf(RED "\nStep 6: Use mmap(2) to allocate a shared buffer of size "
                "equal to 1 page. Initialize the buffer and print the new "
                "mapping information that has been created.\n" RESET);
        press_enter();
        heap_shared_buf = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (heap_shared_buf == MAP_FAILED)
                die("mmap shared");
        memset(heap_shared_buf, 0, buffer_size);
        show_maps();
        p = fork();
        if (p < 0)
                die("fork");
        if (p == 0) {
                child();
                return 0;
        }
        parent(p);
        if (-1 == close(fd))
                perror("close");
        return 0;
}
