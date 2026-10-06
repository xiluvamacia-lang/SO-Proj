#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

#include "parser.h"
#include "datacenter.h"
#include "constants.h"

static int filter_conf(const struct dirent *entry) {
    size_t len = strlen(entry->d_name);
    return (len > 5 && strcmp(entry->d_name + len - 5, ".conf") == 0);
}

static int compare_alpha(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
}

static void process_conf_file(DataCenter *dc, int fd) {
    int running = 1;

    while (running) {
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

static int process_conf_files(DataCenter *dc, const char *input_dir) {
    struct dirent **entries = NULL;

    int n = scandir(input_dir, &entries, filter_conf, compare_alpha);
    if (n < 0) {
        perror("scandir");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        char path[MAX_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/%s", input_dir, entries[i]->d_name);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            free(entries[i]);
            continue;
        }

        process_conf_file(dc, fd);
        close(fd);
        free(entries[i]);
    }

    free(entries);
    return 0;
}

int main(int argc, char **argv) {
    DataCenter dc;
    datacenter_init(&dc);

    if (argc != 6) {
        fprintf(stderr, "Usage: %s <servers> <ram> <disk> <cpus> <input_dir>\n", argv[0]);
        return 1;
    }

    size_t servers, ram, disk;
    double cpu;
    const char *input_dir = argv[5];

    if (!path_exists(input_dir)) {
        fprintf(stderr, "Invalid input directory.\n");
        return 1;
    }

    if (parse_size_t_arg(argv[1], &servers) != 0 ||
        parse_size_t_arg(argv[2], &ram) != 0 ||
        parse_size_t_arg(argv[3], &disk) != 0 ||
        parse_double_arg(argv[4], &cpu) != 0) {
        fprintf(stderr, "Invalid command line arguments.\n");
        return 1;
    }

    Resources resources = { .ram = ram, .disk = disk, .cpu = cpu };

    if (datacenter_configure(&dc, servers, &resources) != 0) {
        fprintf(stderr, "Failed to configure Data Center.\n");
        return 1;
    }

    if (process_conf_files(&dc, input_dir) != 0) {
        fprintf(stderr, "Failed to process configuration files.\n");
        datacenter_destroy(&dc);
        return 1;
    }

    datacenter_destroy(&dc);
    return 0;
}
