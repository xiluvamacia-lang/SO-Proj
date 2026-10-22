#ifndef THREADPOOL__H
#define THREADPOOL__H

#include "datacenter.h"

/**
 * Inicializa a fila de trabalho e cria as worker threads.
 *
 * @param num_threads Número de threads a criar no pool.
 * @param dc Ponteiro para o DataCenter partilhado.
 */
void threadpool_init(int num_threads, DataCenter *dc);

/**
 * Adiciona um ficheiro de configuração à fila de trabalho.
 *
 * @param filepath Caminho para o ficheiro .conf.
 */
void threadpool_add_task(const char *filepath);

/**
 * Sinaliza o fim de novas tarefas e aguarda a conclusão de todas as threads.
 */
void threadpool_wait(void);

/**
 * Destrói as estruturas de sincronização e liberta a memória do pool.
 */
void threadpool_destroy(void);

#endif // THREADPOOL__H