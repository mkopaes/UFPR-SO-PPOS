#include "../include/queue.h"
#include <stdio.h>

int queue_size (queue_t *queue){
    queue_t *aux = queue;
    int size = 1;

    if(aux == NULL)
        return 0;

    while(1){
        if(aux->next == queue)
            break;
        aux = aux->next;
        size++;
    }

    return size;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
    queue_t *aux = queue;

    printf("%s: [", name);
    if(!aux) print_elem(aux);
    else {
        do{
            print_elem(aux);
            if(aux->next != queue)
                printf(" ");
            aux = aux->next;
        } while(aux != queue);
    } 
    printf("]\n");
}

int queue_append (queue_t **queue, queue_t *elem){
    queue_t *aux;

    // Testa se a fila existe
    if(!queue){
        fprintf(stderr, "Erro. Fila inexistente.\n");
        return -1;
    }

    // Testa se elemento existe
    if(!elem){
        fprintf(stderr, "Erro. Elemento inexistente.\n");
        return -2;
    }

    // Testa se elemento está em outra lista
    if(elem->prev){
        fprintf(stderr, "Erro. Elemento em outra fila.\n");
        return -3;
    }

    // Se for o primeiro elemento
    if(*queue == NULL) {
        *queue = elem;
        elem->next = elem;
        elem->prev = elem;
    } else {
        aux = *queue;       // Recebe o primeiro elemento
        aux->prev = elem;
        elem->next = aux;

        // Faça aux ser o último elemento da lista
        while (aux->next != *queue){
            aux = aux->next;
        }
        aux->next = elem;
        elem->prev = aux;
    }

    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem) {
    queue_t *aux;
    int found = 0;

    // Testa se a fila existe
    if(!queue){
        fprintf(stderr, "Erro. Fila inexistente.\n");
        return -1;
    }

    // Testa se fila vazia
    if(!queue_size(*queue)){
        fprintf(stderr, "Erro. Fila vazia.\n");
        return -4;
    }

    // Testa se elemento existe
    if(!elem){
        fprintf(stderr, "Erro. Elemento inexistente.\n");
        return -2;
    }

    // Testa se o elemento esta na fila
    aux = *queue;
    do{
        if(aux == elem) { // Achamos
            found = 1;  
        } else {
            aux = aux->next;
        }
    } while (!found && aux != *queue);

    if(!found){
        fprintf(stderr, "Erro. Elemento nao pertence a fila.\n");
        return -3;
    }

    // Removendo o elemento

    //Fila com 1 elemento só
    if(aux->next == aux || aux->prev == aux){   
        aux->prev = NULL;
        aux->next = NULL;
        *queue = NULL;
    } 
    
    // Fila com + de 1 elemento
    else {                
        // Se removemos o cabeçalho, atualiza o ponteiro
        if(aux == *queue) 
            *queue = aux->next;

        aux->next->prev = aux->prev;
        aux->prev->next = aux->next;
        
        
        // Corta a ligação com a lista
        aux->prev = NULL;
        aux->next = NULL;
    }

    return 0;
}