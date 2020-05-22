#include "music_queue.h"

void MQ_internal_remove(MQ_elt_t *e);

void MQ_init(MQ_t * q){
    q->head = NULL;
    q->tail = NULL;
}

//NOTE: I Destroy all char * here too.
void MQ_deinit(MQ_t *q){
    MQ_elt_t * cur = q->head;
    while(cur){
        MQ_elt_t *temp = cur;
        cur = cur->next;
        MQ_internal_remove(temp);
    }
}

void MQ_add(MQ_t *q, char *id, char *play_loc, char *name){
    MQ_elt_t * n = malloc(sizeof(*n));
    n->next = NULL;
    n->prev = q->tail;
    n->ID = id;
    n->play_loc = play_loc;
    n->name = name;
    if(q->tail){
        q->tail->next = n;
        q->tail = n;
    } else {
        q->head = n;
        q->tail = n;
    }
}

void MQ_remove(MQ_t *q, int pos){
    MQ_elt_t *cur = q->head;
    int i;
    for(i = 0; i < pos, cur; i++, cur = cur->next){
        ;
    }
    if(cur){
        if(cur->prev) cur->prev->next = cur->next;
        if(cur->next) cur->next->prev = cur->prev;
        MQ_internal_remove(cur);
    }

}

void MQ_internal_remove(MQ_elt_t * e){
    free(e->ID);
    free(e->name);
    free(e->play_loc);
    free(e);
}

void MQ_print(MQ_t *q){
    MQ_elt_t *cur = q->head;
    while(cur){
        printf("ITEM: %s, %s, %s\n", cur->ID, cur->name, cur->play_loc);
        cur = cur->next;
    }
}

void MQ_empty(MQ_t *q){
    if(q->head){
        MQ_deinit(q);
        MQ_init(q);
    }
}