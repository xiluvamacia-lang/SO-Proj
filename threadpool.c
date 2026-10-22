#define _XOPEN_SOURCE 700

#include "threadpool.h"
#include "parser.h"
#include "datacenter_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_QUEUE_SIZE 256

typedef struct {
    char *filepaths[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    int done; // 1 quando não houver mais ficheiros a adicionar
    pthread_mutex_t mutex;
    pthread_cond_t cond_work;
} WorkQueue;

static WorkQueue queue;
static pthread_t *threads = NULL;
static int total_threads = 0;
static DataCenter *global_dc = NULL;

static void process_conf_file(DataCenter *dc, int fd) {
    int running = 1;

    while (running) {
        // Chamado fora de qualquer trinco ativo na thread consumidora
        reap_finished_vms(dc);

        switch (get_next_command(fd)) {
            case CMD_DEFINE: {
                VMType vmtype;
                if (parse_define(fd, &vmtype) != 0) {
                    fprintf(stderr, "Invalid define command.\n");
                    continue;
                }
                if (datacenter_define_VM(dc, &vmtype) != 0) {
                    fprintf(stderr, "Failed to define VM.\n");
                    continue;
                }
                printf("VM successfully defined!\n");
                break;
            }

            case CMD_RESERVE: {
                Reservation reservation = {0};
                size_t num_items = parse_reserve(fd, &reservation, MAX_RESERVATIONS_ITEMS);
                if (num_items == 0) {
                    fprintf(stderr, "Invalid reserve command.\n");
                    continue;
                }
                if (datacenter_reserve(dc, &reservation) != 0) {
                    fprintf(stderr, "Failed to reserve VMs.\n");
                    continue;
                }
                printf("Reservation made successfully!\n");
                break;
            }

            case CMD_EXECUTE: {
                char id[MAX_STRING_SIZE];
                if (parse_execute(fd, id) != 0) {
                    fprintf(stderr, "Invalid execute command.\n");
                    continue;
                }
                if (datacenter_execute(dc, id) != 0) {
                    fprintf(stderr, "Failed to execute reservation.\n");
                    continue;
                }
                printf("Finished reservation execution!\n");
                break;
            }

            case CMD_LIST:
                if (datacenter_list(dc) != 0) {
                    fprintf(stderr, "Failed to list VMs.\n");
                }
                break;

            case CMD_WAIT: {
                unsigned int delay;
                if (parse_wait(fd, &delay) != 0) {
                    fprintf(stderr, "Invalid wait command.\n");
                    continue;
                }
                datacenter_wait(delay);
                break;
            }

            case CMD_INVALID:
                fprintf(stderr, "Invalid Command.\n");
                break;

            case CMD_HELP:
                printf(
                    "Available commands:\n"
                    " D <VM_TYPE_ID> <INPUT_FOLDER> <EXECUTABLE_PATH> <RAM> <DISK> <VCPU>\n"
                    " R <RESERVATION_ID> [<VM_TYPE_ID> <COUNT> <SERVER_ID>]+\n"
                    " A <RESERVATION_ID>\n"
                    " L\n"
                    " E <DELAY_MS>\n"
                    " H\n"
                );
                break;

            case CMD_EMPTY:
                break;

            case EOC:
                running = 0;
                break;
        }
    }
}

static void *worker_thread_func(void *arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&queue.mutex);

        while (queue.count == 0 && !queue.done) {
            pthread_cond_wait(&queue.cond_work, &queue.mutex);
        }

        if (queue.count == 0 && queue.done) {
            pthread_mutex_unlock(&queue.mutex);
            break;
        }

        char *filepath = queue.filepaths[queue.head];
        queue.head = (queue.head + 1) % MAX_QUEUE_SIZE;
        queue.count--;

        pthread_mutex_unlock(&queue.mutex);

        int fd = open(filepath, O_RDONLY);
        if (fd >= 0) {
            process_conf_file(global_dc, fd);
            close(fd);
        } else {
            perror("Failed to open conf file");
        }

        free(filepath);
    }

    return NULL;
}

void threadpool_init(int num_threads, DataCenter *dc) {
    global_dc = dc;
    total_threads = num_threads;

    queue.head = 0;
    queue.tail = 0;
    queue.count = 0;
    queue.done = 0;
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.cond_work, NULL);

    threads = malloc((size_t)num_threads * sizeof(pthread_t));
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread_func, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
}

void threadpool_add_task(const char *filepath) {
    pthread_mutex_lock(&queue.mutex);

    queue.filepaths[queue.tail] = strdup(filepath);
    queue.tail = (queue.tail + 1) % MAX_QUEUE_SIZE;
    queue.count++;

    pthread_cond_signal(&queue.cond_work);
    pthread_mutex_unlock(&queue.mutex);
}

void threadpool_wait(void) {
    pthread_mutex_lock(&queue.mutex);
    queue.done = 1;
    pthread_cond_broadcast(&queue.cond_work);
    pthread_mutex_unlock(&queue.mutex);

    for (int i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}

void threadpool_destroy(void) {
    pthread_mutex_destroy(&queue.mutex);
    pthread_cond_destroy(&queue.cond_work);
    free(threads);
    threads = NULL;
}