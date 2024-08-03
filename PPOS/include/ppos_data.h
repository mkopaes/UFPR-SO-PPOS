// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.5 -- Março de 2023

// Alterações realizadas por:
// Murilo Paes GRR20190158
// Versão: P13 -- 29 de Julho de 2024

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>

#define SYSTEM_TASK         1
#define USER_TASK           0
#define TASK_READY          0
#define TASK_FINISHED       1
#define TASK_SUSPENDED      2
#define ERROR_STACK         -1
#define ERROR_APPEND        -2
#define ERROR_TASKNULL      -3
#define ERROR_ALLOC         -4


// Estrutura que define um Task Control Block (TCB)
typedef struct task_t 
{
	struct task_t *prev, *next ;	// ponteiros para usar lib queue.h
	int id ;				        // identificador da tarefa
	ucontext_t context ;			// contexto armazenado da tarefa
	short status ;			        // pronta, rodando, suspensa, ...
	int static_priority;            // prioridade estática da tarefa
	int dynamic_priority;           // prioridade dinâmica da tarefa
	short type;                     // task é de Sistema ou Usuário
	unsigned int init_time;         // horário da criação da task
	unsigned int cpu_time;          // tempo de uso de CPU
	unsigned int activations;       // número de ativações da task
	struct task_t** tasks_on_hold;  // fila de tasks esperando o fim desta
	unsigned int wakeup_time;       // horário em que a task deve ser desperta
	struct task_t** current_queue;  // endereço da fila em que a task está
} task_t ;

// estrutura que define um semáforo
typedef struct semaphore_t
{
	int counter;                    // Contador de 'vagas'
	int lock;                       // Flag de bloqueio
	int destroyed;                  // Flag de semáforo destruído
	task_t** queue;
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
	volatile int locked;            // Estado do mutex: 0 (livre) ou 1 (bloqueado)
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{

} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct mqueue_t
{ 
	void *buffer;                   // Buffer para armazenar as mensagens
	int max_msgs;                   // Capacidade máxima de mensagens na fila
	int msg_size;                   // Tamanho de cada mensagem
	int head;                       // Índice do início da fila
	int tail;                       // Índice do fim da fila
	int count;                      // Número de mensagens na fila
	mutex_t mutex;                  // Mutex para sincronização
	semaphore_t empty;              // Semáforo para indicar espaço vazio na fila
	semaphore_t full;               // Semáforo para indicar fila cheia
} mqueue_t ;

task_t* getCurrentTask();

#endif

