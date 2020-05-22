#ifndef MUSIC_QUEUE_H
#define MUSIC_QUEUE_H

#include <stdlib.h>
#include <stdio.h>
typedef struct MQ_elt_str{
    struct MQ_elt_str * next;
    struct MQ_elt_str * prev;
    char *ID;
    char *play_loc;
    char *name;
} MQ_elt_t;

typedef struct{
    MQ_elt_t * head;
    MQ_elt_t * tail;
    //Other stuff later?
} MQ_t;

void MQ_init(MQ_t * q);
void MQ_deinit(MQ_t *q);
void MQ_add(MQ_t *q, char *id, char *play_loc, char *name);
void MQ_remove(MQ_t *q, int pos);
void MQ_print(MQ_t *q);
void MQ_empty(MQ_t *q);



#endif