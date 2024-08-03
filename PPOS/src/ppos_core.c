// PingPongOS - PingPong Operating System
// Murilo Paes GRR20190158
// Versão: P13 -- 29 de Julho de 2024

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

#include "../include/ppos.h"
#include "../include/queue.h"

// Operating system check
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#define STACKSIZE 64*1024   // tamanho de pilha das threads
#define QUANTUM 20
#define AGING -1

// #define DEBUG_INIT
// #define DEBUG_QUANTUM
// #define DEBUG_SWITCH
// #define DEBUG_SLEEP
// #define DEBUG_SEM
// #define DEBUG_EXIT

struct sigaction action;        // estrutura que define um tratador de sinal
struct itimerval timer;         // estrutura de inicialização do timer
unsigned int userTasks, quantum, system_time, cpu_init, total_cpu_time;
int id;
task_t mainTask, dispatcherTask;
task_t *currentTask, **queueReady, **queueSleep;

// Funções internas ============================================================

// Função tratadora do sinal, decrementa o quantum e checa se chegou a zero
void timer_signal_handler (int signum){
    system_time++;
    if(currentTask->type == USER_TASK){
        quantum--;
        if(!quantum){
            #ifdef DEBUG_QUANTUM
            printf("task %d: atingiu quantum\n", currentTask->id);
            #endif
            task_yield();
        }
    }
}

// Entra em uma critical section (método OP Atômica)
void enter_cs(int *lock){
    while (__sync_fetch_and_or (lock, 1));
}

// Sai da critical section (método OP Atômica)
void leave_cs(int *lock){
    (*lock) = 0;
}

/* Procura a prioridade mais forte (valor mais negativo) na fila 'queue' de tarefas
Retorna o ponteiro para a tarefa prioritária. */
task_t *search_prio(task_t **queue, int size){
    task_t *aux = (*queue);
    task_t *stronger_prio = aux;
    
    for(int i = 1; i < size; i++){
        aux = aux->next;
        if(aux->dynamic_priority < stronger_prio->dynamic_priority)
            stronger_prio = aux;
    }
    
    return stronger_prio;
}

/* Aumenta a prioridade dinâmica das tarefas na fila 'queue' de tamanho 'size' num valor
igual a 'aging_value'. As prioridades nunca serão inferiores a -20 ou superiores a +20 */
void task_aging(task_t **queue, int size, int aging_value){
    task_t *aux = (*queue);
    
    for(int i = 0; i < size; i++){
        if(aux->dynamic_priority >= (-20 - aging_value))
            aux->dynamic_priority += aging_value;
        aux = aux->next;
    }
}

/* Percorre a fila de tarefas dormindo procurando se alguma deve ser acordada */
void sleeping_queue_checker(){
    unsigned int size = queue_size((queue_t*) *queueSleep);
    unsigned int actual_time = systime();
    task_t *aux = *queueSleep;

    // ------- Fila vazia
    if(!size) {return;}
    
    // ------- Fila não vazia
    for(int i = 0; i < size; i++){
        if(aux->wakeup_time == actual_time && aux->wakeup_time != 0){
            if(size == 1){
                task_awake(aux, queueSleep);
            } else {
                task_t *task_to_wake = aux;
                aux = aux->next;
                task_awake(task_to_wake, queueSleep);
            }
        } else {
            aux = aux->next;
        }
    }
}

/* Scheduler do sistema, o sistema de escalonador atual é: Task aging (α = -1)
   Retorna um ponteiro para tarefa (se houver), do contrário retorna NULL */
task_t *scheduler(){
    if(queueReady){
        int size = queue_size((queue_t*) (*queueReady));
        task_t *aux = search_prio(queueReady, size);

        queue_remove((queue_t**) queueReady, (queue_t*) aux);
        size--;

        task_aging(queueReady, size, AGING);

        return aux;
    }
    return NULL;
}

void dispatcher(){
    queue_remove((queue_t**) queueReady, (queue_t*) currentTask);

    while(userTasks > 0){
        sleeping_queue_checker();
        if(queue_size((queue_t*) *queueReady)){ // Caso haja alguma task pronta
            task_t *next_task = scheduler();
        
            if(next_task){
                quantum = QUANTUM;
                task_switch(next_task);
            }
        }
    }
    task_exit(0);
}


// Funções gerais ==============================================================

void ppos_init (){
    // ------- Inicialização das variáveis globais
    id = 0; userTasks = 0; system_time = 0; cpu_init = 0; total_cpu_time = 0;
    setvbuf (stdout, 0, _IONBF, 0); // Desativa o buffer da saida padrao (stdout)

    // ------- Inicialização das filas usadas
    queueReady = (task_t**) malloc(sizeof(task_t*));
    queueSleep = (task_t**) malloc(sizeof(task_t*));
    *(queueReady) = NULL;
    *(queueSleep) = NULL;

    // ------- Inicialização das tasks iniciais
    dispatcherTask.type = SYSTEM_TASK;
    task_init(&dispatcherTask, (void*) *dispatcher, NULL);  // Cria o Dispatcher
    task_init(&mainTask, NULL, NULL);
    
    #ifdef DEBUG_INIT
    printf("ppos_init  : task main e dispatcher criados.\n");
    #endif
    
    // ------- Registro de ação para SIGALRM
    action.sa_handler = timer_signal_handler;
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0 ;

    if (sigaction (SIGALRM, &action, 0) < 0){
        perror ("Erro em sigaction: ") ;
        exit (1) ;
    }

    // ------- Configuração do temporizador
    timer.it_value.tv_usec = 1000;      // primeiro disparo, em micro-segundos
    timer.it_interval.tv_usec = 1000;   // disparos subsequentes, em micro-segundos

    if (setitimer (ITIMER_REAL, &timer, 0) < 0){
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }

    #ifdef DEBUG_INITDEBUG_INIT
    printf("ppos_init  : temporizador definido.\n");
    #endif

    // ------- Demais operações
    currentTask = &mainTask;
    currentTask->activations++;
    quantum = QUANTUM;

    #ifdef DEBUG_INIT
    printf("ppos_init  : sistema iniciado com sucesso.\n");
    #endif
}

// Gerência de tarefas =========================================================

task_t* getCurrentTask(){
    return currentTask;
}

int task_init (task_t *task, void (*start_routine)(void *), void *arg) {
    // ------- ID
    task->id = id;

    #ifdef DEBUG_INIT
    printf("task_init(%d): id criado com sucesso.\n", id);
    #endif
    
    // ------- Context
    getcontext(&task->context);

    #ifdef DEBUG_INIT
    printf("task_init(%d): contexto criado com sucesso.\n", id);
    #endif

    char *stack = malloc (STACKSIZE);
    if(stack){
        task->context.uc_stack.ss_sp = stack ;
        task->context.uc_stack.ss_size = STACKSIZE ;
        task->context.uc_stack.ss_flags = 0 ;
        task->context.uc_link = 0 ;   
    } else {
        perror ("Erro na criação da pilha\n") ;
        return ERROR_STACK;
    }

    #ifdef DEBUG_INIT
    printf("task_init(%d): stack criada com sucesso.\n", id);
    #endif
    
    // ------- Priorities
    task->static_priority = 0;
    task->dynamic_priority = 0;

    // ------- System task or User task ?
    if(task->type != SYSTEM_TASK){
        task->type = USER_TASK;
        userTasks++;
    }

    // ------- Initial function and parameters
    if(start_routine){
        makecontext(&task->context, (void(*)(void)) start_routine, 1, arg);
        
        if(queue_append((queue_t **) queueReady, (queue_t*) task) != 0)
            return ERROR_APPEND;
        task->current_queue = queueReady;

        #ifdef DEBUG_INIT
        printf("task_init(%d): task inserida na fila\n", task->id);
        #endif 
    }

    // ------- Starting others variables
    task->tasks_on_hold = (task_t**) malloc(sizeof(task_t**));
    *(task->tasks_on_hold) = NULL;
    task->status = TASK_READY;

    // ------- Time related
    task->init_time = systime();
    task->cpu_time = 0;
    task->activations = 0;
    task->wakeup_time = 0;

    // ------- Global variables
    id++;

    #ifdef DEBUG_INIT
    printf("task_init(%d): task criada com sucesso\n", task->id);
    printf("task_init   : User Tasks = %d\n", userTasks);
    #endif

    return task->id;
}		

int task_id () {
    return currentTask->id;
}

void task_exit (int exit_code){
    // ------- Acordar tarefas dependentes
    if(currentTask->id){
        task_t *aux = *(currentTask->tasks_on_hold);
        while(aux){
            task_awake(aux, currentTask->tasks_on_hold);
            aux = *(currentTask->tasks_on_hold);
        }  
    }

    // ------- Sumarização de tempo / uso CPU
    currentTask->cpu_time += systime() - cpu_init;

    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", 
            currentTask->id, systime() - currentTask->init_time, 
            currentTask->cpu_time, currentTask->activations);

    total_cpu_time += currentTask->cpu_time;  

    // ------- Tarefas Gerenciais
    currentTask->status = TASK_FINISHED;
    free(currentTask->tasks_on_hold);
    //free(currentTask->context.uc_stack.ss_sp);
    if(currentTask->type == USER_TASK) {userTasks--;}

    #ifdef DEBUG_EXIT
    printf("task_exit(%d): tarefa encerrada\n", currentTask->id);
    printf("task_exit(%d): User Tasks = %d\n", currentTask->id, userTasks);
    #endif
    
    if(currentTask->id)
        task_switch(&dispatcherTask);
    else {
        #ifdef DEBUG
        printf("Uso total de CPU: %d ms\n", total_cpu_time);
        #endif
        exit(exit_code);
    }
}

int task_switch (task_t *task){   
    if(task){
        #ifdef DEBUG_SWITCH
        printf("task_switch: trocando contexto %d -> %d\n", currentTask->id, task->id);
        #endif

        task_t *aux = currentTask;
        currentTask = task;
        currentTask->activations++;

        aux->cpu_time += systime() - cpu_init;          // Atualizando o tempo de CPU da task antiga
        swapcontext(&(aux->context), &(task->context));
        cpu_init = systime();                           // Iniciando contador de tempo de CPU da task nova

        return 0;
    } else {
        printf("Task inexistente.\n");
        return -1; 
    }
}

void task_suspend (task_t **queue){
    currentTask->status = TASK_SUSPENDED;

    if(queue == NULL)
        queue = queueSleep;
    
    if(queue_append((queue_t**) queue, (queue_t*) currentTask)){
        perror ("Erro ao tentar inserir na fila apontada.\n") ;
        return;
    }
    
    currentTask->current_queue = queue;

    #ifdef DEBUG_SLEEP
    printf("task_suspend(%d): foi suspensa.\n", currentTask->id);
    #endif

    task_switch(&dispatcherTask);
}

void task_awake (task_t *task, task_t **queue){
    if(!task || !queue_size((queue_t*) *queue))
        return;

    queue_remove((queue_t**) queue, (queue_t*) task);
    queue_append((queue_t**) queueReady, (queue_t*) task);

    task->status = TASK_READY;
    task->current_queue = queueReady;
        
    #ifdef DEBUG_SLEEP
    printf("task_awake(%d): task foi acordada\n", task->id);
    #endif
}

// Operações de escalonamento ==================================================

void task_yield (){
    #ifdef DEBUG_SLEEP
    printf("task_yield(%d): trocando para dispatcher\n", currentTask->id);
    #endif

    currentTask->status = TASK_READY;
    currentTask->dynamic_priority = task_getprio(currentTask);

    queue_append((queue_t**) queueReady, (queue_t*) currentTask);
    currentTask->current_queue = queueReady;
    task_switch(&dispatcherTask);
}

void task_setprio (task_t *task, int prio){
    if(prio >= -20 && prio <= 20){ // -20 (prioridade maior) <= prio <= +20 (prioridade menor)
        if(task){
            task->static_priority = prio;
            task->dynamic_priority = prio;
        } else {
            currentTask->static_priority = prio;
            currentTask->dynamic_priority = prio;
        }
    } else {
        printf("Erro: prioridade definida está fora dos limites (-20 a +20)\n");
    }
}

int task_getprio (task_t *task){
    if(task)
        return task->static_priority;
    else
        return currentTask->static_priority;
}

// operações de gestão do tempo ================================================

unsigned int systime () {
    return system_time;
}

void task_sleep (int t) {
    if(t > 0){
        currentTask->wakeup_time = systime() + t;
        currentTask->status = TASK_SUSPENDED;
        queue_append((queue_t**) queueSleep, (queue_t*) currentTask);
        currentTask->current_queue = queueSleep;
        task_switch(&dispatcherTask);
    }
}

// operações de sincronização ==================================================

int task_wait (task_t *task){
    if(!task) return ERROR_TASKNULL;
    if(task->status == TASK_FINISHED) return ERROR_TASKNULL;
    
    #ifdef DEBUG
    printf("task_wait(%d): task suspendida, aguardando %d encerrar.\n", currentTask->id, task->id);
    #endif

    task_suspend(task->tasks_on_hold);
    return(task->id);
}

int sem_init (semaphore_t* s, int value){
    if(!s) {s = (semaphore_t*) malloc (sizeof(semaphore_t));}
    s->counter = value;
    s->lock = 0;
    s->destroyed = 0;
    s->queue = (task_t**) malloc(sizeof(task_t*));

    if(!s->queue){
        perror ("Erro na criação do semáforo\n") ;
        return -1;
    }
    
    *(s->queue) = NULL;

    #ifdef DEBUG_SEM
    printf("sem_init: semaforo iniciado.\n");
    #endif

    return 0;
}

int sem_down (semaphore_t *s){
    // ------- Verificação de existência
    if(!s || s->destroyed){
        return -1;
    }

    #ifdef DEBUG_SEM
    printf("sem_down(%d): número de vagas = %d\n", currentTask->id, s->counter);
    printf("sem_down(%d): solicitando semáforo.\n", currentTask->id);
    #endif

    // ------- Tratamento da racing condition
    enter_cs(&s->lock);
    s->counter--;
    leave_cs(&s->lock);

    #ifdef DEBUG_SEM
    printf("sem_down(%d): número de vagas = %d\n", currentTask->id, s->counter);
    #endif

    // ------- Execução do sem_down
    if(s->counter < 0){
        #ifdef DEBUG_SEM
        printf("sem_down(%d): task suspendida\n", currentTask->id);
        #endif
        task_suspend(s->queue);
        if(s->destroyed)
            return -1;
    }
    return 0;
}

int sem_up (semaphore_t *s){
    // ------- Verificação de existência
    if(!s || s->destroyed){
        return -1;
    }

    #ifdef DEBUG_SEM
    printf("sem_up(%d)  : número de vagas = %d\n", currentTask->id, s->counter);
    printf("sem_up(%d)  : liberando semáforo\n", currentTask->id);
    #endif

    // ------- Tratamento da racing condition
    enter_cs(&s->lock);
    s->counter++;
    leave_cs(&s->lock);

    #ifdef DEBUG_SEM
    printf("sem_up(%d)  : número de vagas = %d\n", currentTask->id, s->counter);
    #endif

    // ------- Execução do sem_up
    if(s->counter <= 0){
        #ifdef DEBUG_SEM
        printf("sem_up(%d): task acordada.\n", (*(s->queue))->id);
        #endif
        task_awake(*(s->queue), s->queue);
    }
    return 0;
}

int sem_destroy (semaphore_t *s){
    // ------- Verificação de existência
    if(!s){
        return -1;
    }

    // ------- Acorda as tasks presas no semáforo
    s->destroyed = 1;
    while (*(s->queue)) {
        task_awake(*(s->queue), s->queue);
    }

    // ------- Destrói o semáforo
    s = NULL;

    #ifdef DEBUG_SEM
    printf("sem_destroy:  semáforo desstruído\n");
    #endif

    return 0;
}

// inicializa um mutex (sempre inicialmente livre)
int mutex_init (mutex_t *m){
    m->locked = 0;
    return 0;
}

// requisita o mutex
int mutex_lock (mutex_t *m){
    while (__sync_lock_test_and_set(&m->locked, 1)); // Busy waiting
    return 0;
}

// libera o mutex
int mutex_unlock (mutex_t *m){
    __sync_lock_release(&m->locked);
    return 0;
}

// "destroi" o mutex, liberando as tarefas bloqueadas
int mutex_destroy (mutex_t *m){
    m->locked = 0;
    return 0;
}

// operações de comunicação ====================================================

int mqueue_init (mqueue_t *queue, int max, int size){
    // ------- Teste de argumentos inválidos
    if (max <= 0 || size <= 0) {
        return -1;
    }
    
    // ------- Buffer de mensagens
    queue->buffer = malloc(max * size);
    if (queue->buffer == NULL) {
        return -1;
    }

    // ------- Alocação do mutex
    if (mutex_init(&queue->mutex) != 0) {
        free(queue->buffer);
        return -1;
    }

    // ------- Alocação semáforo indicativo de espaço vazio
    if (sem_init(&queue->empty, max) != 0) {
        mutex_destroy(&queue->mutex);
        free(queue->buffer);
        return -1;
    }

    // ------- Alocação semáforo indicativo de fila cheia
    if (sem_init(&queue->full, 0) != 0) {
        sem_destroy(&queue->empty);
        mutex_destroy(&queue->mutex);
        free(queue->buffer);
        return -1;
    }

    // ------- Demais variáveis
    queue->max_msgs = max;
    queue->msg_size = size;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    return 0;
}

int mqueue_send (mqueue_t *queue, void *msg){
    if(sem_down(&queue->empty))
        return -1;

    mutex_lock(&queue->mutex);
    
    // ------- Envio da mensagem no Buffer
    memcpy((char*)queue->buffer + (queue->tail * queue->msg_size), msg, queue->msg_size);
    queue->tail = (queue->tail + 1) % queue->max_msgs;
    queue->count++;
    
    mutex_unlock(&queue->mutex);
    
    sem_up(&queue->full); // Indica que há uma mensagem nova na fila
    
    return 0;
}

int mqueue_recv (mqueue_t *queue, void *msg){
    if(sem_down(&queue->full))
        return -1;
    
    mutex_lock(&queue->mutex);

    // ------- Recebimento da mensagem no Buffer
    memcpy(msg, (char*)queue->buffer + (queue->head * queue->msg_size), queue->msg_size);
    queue->head = (queue->head + 1) % queue->max_msgs;
    queue->count--;

    mutex_unlock(&queue->mutex);
    
    sem_up(&queue->empty); // Indica que há um novo espaço vazio na fila
    
    return 0;
}

// destroi a fila, liberando as tarefas bloqueadas
int mqueue_destroy (mqueue_t *queue){
    // ------- Produtores são acordados
    sem_destroy(&queue->empty);

    // ------- Consumidores são acordados
    sem_destroy(&queue->full);

    mutex_destroy(&queue->mutex);
    free(queue->buffer);

    return 0;
}

// informa o número de mensagens atualmente na fila
int mqueue_msgs (mqueue_t *queue){
    //mutex_lock(&queue->mutex);
    int count = queue->count;
    //mutex_unlock(&queue->mutex);
    return count;
}


// ========================= Funções não implementadas =========================

// operações de sincronização ==================================================

// inicializa uma barreira para N tarefas
// int barrier_init (barrier_t *b, int N) ;

// espera na barreira
// int barrier_wait (barrier_t *b) ;

// destrói a barreira, liberando as tarefas
// int barrier_destroy (barrier_t *b) ;
