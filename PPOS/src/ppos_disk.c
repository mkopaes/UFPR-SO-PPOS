#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "../include/ppos_disk.h"
#include "../include/queue.h"
#include "../disk/disk.h"

#define READ            0
#define WRITE           1
#define DISK_NAME       "disk.dat"	// arquivo com o conteúdo do disco

// #define DEBUG

struct sigaction sig;
disk_t *disk;
task_t diskDriverTask;
semaphore_t sem;
int taskDone;

// Funções internas ============================================================
void sigusr1_handler(int signum){
    taskDone = 1;
    task_awake(&diskDriverTask, diskDriverTask.current_queue);
}

void imprimeRequest(request_t *request){
    printf("Bloco: %d | Tipo : %d | Task : %d\n", \
    request->block, request->type, request->owner->id);
}

void diskDriverBody (task_t **queueSleep){
    request_t *request = NULL;
    while (1){
        sem_down(&sem);

        // se foi acordado devido a um sinal do disco
        if (taskDone){
            // acorda a tarefa cujo pedido foi atendido
            task_awake(request->owner, request->owner->current_queue);
            taskDone = 0;
        }

        int diskStatus = disk_cmd (DISK_CMD_STATUS, 0, 0) ;
        int requestQueueSize = queue_size((queue_t*) *(disk->requestQueue));

        if (diskStatus == DISK_STATUS_IDLE && requestQueueSize > 0){
            request = *(disk->requestQueue);
            queue_remove((queue_t**) disk->requestQueue, (queue_t*) request);
            // imprimeRequest(request);

            switch (request->type){
            case READ:
                disk_cmd (DISK_CMD_READ, request->block, request->buffer);
                break;
            case WRITE:
                disk_cmd (DISK_CMD_WRITE, request->block, request->buffer);
                break;
            }
        }
 
        // libera o semáforo de acesso ao disco
        sem_up(&sem);

        // suspende a tarefa corrente (retorna ao dispatcher)
        task_suspend(NULL);
   }
}

// Funções Gerais ==============================================================
int disk_mgr_init (int *numBlocks, int *blockSize){
    disk = (disk_t*) malloc(sizeof(disk_t));

    //------- Variáveis de disk_cmd
    if(disk_cmd (DISK_CMD_INIT, 0, 0)) return -1;
    
    *numBlocks = disk_cmd (DISK_CMD_DISKSIZE, 0, 0);
    if(numBlocks < 0) return -1;
    disk->numblocks = *numBlocks;

    *blockSize = disk_cmd (DISK_CMD_BLOCKSIZE, 0, 0);
    if(blockSize < 0) return -1;
    disk->blocksize = *blockSize;

    //------- Fila de Pedidos
    disk->requestQueue = (request_t**) malloc(sizeof(request_t*));
    *(disk->requestQueue) = NULL;
    sem_init(&sem, 1);

    // ------- Registro de ação para SIGUSR1
    sig.sa_handler = sigusr1_handler;
    sigemptyset (&sig.sa_mask);
    sig.sa_flags = 0 ;
    if (sigaction (SIGUSR1, &sig, 0) < 0){
        perror ("Erro em sigaction: ") ;
        return -1;
    }

    // ------- Cria a tarefa gerenciadora de disco
    diskDriverTask.type = SYSTEM_TASK;
    task_init(&diskDriverTask, (void*) diskDriverBody, NULL);

    return 0;
}

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer){
    if(block < 0 || block > disk->numblocks - 1) return -1;

    //------- Solicitação
    request_t *request= (request_t*) malloc (sizeof(request_t));
    request->block = block;
    request->buffer = buffer;
    request->type = READ;
    request->owner = getCurrentTask();

    //------- Inclusão na fila
    sem_down(&sem);
    queue_append((queue_t**)disk->requestQueue, (queue_t*) request);

    #ifdef DEBUG
    printf("disk_block_read: request inserida.\n");
    #endif

    if (diskDriverTask.status == TASK_SUSPENDED){
        task_awake(&diskDriverTask, diskDriverTask.current_queue);
    }

    sem_up(&sem); 

    //------- Retorno ao dispatcher
    task_suspend(NULL);

    return 0;
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer){
    if(block < 0 || block > disk->numblocks - 1) return -1;

    //------- Solicitação
    request_t *request= (request_t*) malloc (sizeof(request_t));
    request->block = block;
    request->buffer = buffer;
    request->type = WRITE;
    request->owner = getCurrentTask();

    //------- Inclusão na fila
    sem_down(&sem);
    queue_append((queue_t**)disk->requestQueue, (queue_t*) request);

    if (diskDriverTask.status == TASK_SUSPENDED){
        task_awake(&diskDriverTask, diskDriverTask.current_queue);
    }
 
    sem_up(&sem);
 
    //------- Retorno ao dispatcher
    task_suspend(NULL);

    return 0;
}