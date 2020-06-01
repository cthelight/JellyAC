#ifndef MPLAYER_WRAPPER_H
#define MPLAYER_WRAPPER_H
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>

#define JSMN_HEADER
#include "jsmn/jsmn.h"
#include "music_queue.h"

typedef struct {
    double vol;
    double pos;
    char muted;
    char * play_sess_id;
    char * item;
    char * playlist_item_name;
    long playback_start_ticks;
    char paused;
    char stopped;
    char repeat;
    int percent_pos;
    char has_name;
    MQ_t q;

    //Record times of updates
    long vol_update_time;
    long pos_update_time;
    long perc_pos_update_time;
} mp_state_t;
void* start_mplayer(void *);
void * open_output_fifo(void*);
int play_playlist(char * str, jsmntok_t *t, int num_tok, int start_idx);
void mplayer_wrapper_init();
int quit();
int prev();
int stop();
int next();
int toggle_pause();
int flush_output_buffer();
int get_percent_pos();
double get_time_pos();
double get_vol_level();
void set_time_pos(double sec);
void set_vol_level(double lvl);
void toggle_mute();
void set_repeat_none();
void set_repeat_all();
#endif