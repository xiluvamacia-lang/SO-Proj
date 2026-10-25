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
#include "datacenter_utils.h"
#include "threadpool.h"

static int compare_alpha(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
}

int main(int argc, char **argv) {
    DataCenter dc;
    datacenter_init(&dc);

    if (argc != 8) {
        fprintf(stderr, "Usage: %s <servers> <ram> <disk> <cpus> <colors> <input_dir>\n <max_threads>", argv[0]);
        return 1;
    }

    size_t servers, ram, disk, max_threads; size_t colors; double cpu;
    const char *input_dir = argv[6];

    if (!path_exists(input_dir)) {
        fprintf(stderr, "Invalid input directory.\n");
        return 1;
    }

    if (parse_size_t_arg(argv[1], &servers) != 0 ||
        parse_size_t_arg(argv[2], &ram) != 0 ||
        parse_size_t_arg(argv[3], &disk) != 0 ||
        parse_double_arg(argv[4], &cpu) != 0 ||
        parse_size_t_arg(argv[5], &colors) != 0 || colors == 0 ||
        parse_size_t_arg(argv[7], &max_threads) != 0 || max_threads == 0) {
        fprintf(stderr, "Invalid command line arguments.\n");
        return 1;
    }

    Resources resources = { .ram = ram, .disk = disk, .cpu = cpu };

    if (datacenter_configure(&dc, servers, &resources) != 0) {
        fprintf(stderr, "Failed to configure Data Center.\n");
        return 1;
    }

    dc.colors = colors;
    threadpool_init((int)max_threads, &dc);

    // Procura os ficheiros .conf e envia os caminhos para a fila
    struct dirent **entries = NULL;
    int n = scandir(input_dir, &entries, is_conf_file, compare_alpha);
    if (n >= 0) {
        for (int i = 0; i < n; i++) {
            char path[MAX_PATH_SIZE];
            snprintf(path, sizeof(path), "%s/%s", input_dir, entries[i]->d_name);

            threadpool_add_task(path); // Adiciona à fila

            free(entries[i]);
        }
        free(entries);
    } else {
        perror("scandir");
    }

    // Espera que todas as worker threads terminem o seu processamento
    threadpool_wait();

    while (has_running_vms(&dc)){
        reap_finished_vms(&dc);
        datacenter_wait(10);
    }

    // Limpeza
    threadpool_destroy();

    datacenter_destroy(&dc);
    return 0;
}
