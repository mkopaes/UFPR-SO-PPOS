// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.4 -- Janeiro de 2022

// interface do gerente de disco rígido (block device driver)

#ifndef __DISK_MGR__
#define __DISK_MGR__

#include "../include/ppos.h"

// estruturas de dados e rotinas de inicializacao e acesso
// a um dispositivo de entrada/saida orientado a blocos,
// tipicamente um disco rigido.

// estrutura que representa um pedido de disco
typedef struct {
    struct request_t *prev;     // Solicitação anterior
    struct request_t *next;     // Solicitação seguinte
    task_t* owner;              // ID da task que solicitou o disco
    int block;                  // Número do bloco solicitado
    void *buffer;               // Endereço do Buffer
    int type;                   // Leitura = 0, Escrita = 1
} request_t;

// estrutura que representa um disco no sistema operacional
typedef struct {
    int numblocks ;		        // numero de blocos do disco
    int blocksize ;		        // tamanho dos blocos em bytes
    request_t **requestQueue;   // Fila de solicitações de disco
    // completar com os campos necessarios
} disk_t ;

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize) ;

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) ;

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) ;

#endif