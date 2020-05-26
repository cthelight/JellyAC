#include "mplayer_wrapper.h"
#include "curl_func.h"
#include "music_queue.h"
#include <time.h>
#include <unistd.h>
#define MPLAYER_FIFO "/tmp/mplayer_input"
#define PLAYLIST_LOC "/tmp/playlist.txt"
#define MPLAYER_OUTPUT_FIFO "/tmp/mplayer_output"
//Server, songid, Userid, deviceid, API-key
#define MPW_HTTP_LOC_BASE "http://%s/Audio/%s/universal?UserId=\"%s\"&DeviceId=\"%s\"&MaxStreamingBitrate=140000000&Container=opus%%2Cmp3%%7Cmp3%%2Cflac%%2Cwebma%%2Cwebm%%2Cwav%%2Cogg&TranscodingContainer=ts&TranscodingProtocol=hls&AudioCodec=aac&api_key=\"%s\""

pthread_t fifo_control_thread;
pthread_mutex_t fifo_control_mutex;
pthread_t fifo_output_thread;
pthread_mutex_t fifo_output_mutex;
pthread_t mplayer_thread;
pthread_mutex_t run_mutex;

pthread_mutex_t play_mutex;

pthread_t curl_prog_upd_thread;
pthread_mutex_t state_mutex;

pthread_mutex_t read_prop_mutex;

pthread_mutex_t prog_mutex;

char mplayer_running = 0;
int mp_fifo, mp_out_fifo;
extern char * user_id;
extern char * acc_tok;
extern char device_id[128];
extern const char *server_addr;

MQ_t q;
MQ_elt_t *q_elt;

char play_prev = 0; //Flag indicating mplayer killed to play prev (if existing)
char stopped = 0; //Flag indicating mplayer killed due to stop
char muted = 0; //Tracks current mute state (initially not muted)
char go_back = 0; //Flag to actually go to previous track, not restart


mp_state_t state;

void mplayer_wrapper_init(){
    MQ_init(&q);
    pthread_mutex_init(&run_mutex, NULL);
    pthread_mutex_init(&fifo_control_mutex, NULL);
    pthread_mutex_init(&fifo_output_mutex, NULL);
    pthread_mutex_init(&read_prop_mutex, NULL);
    pthread_mutex_init(&state_mutex, NULL);
    pthread_mutex_init(&prog_mutex, NULL);
    pthread_mutex_init(&play_mutex, NULL);
    pthread_create(&fifo_control_thread, NULL, open_control_fifo, NULL);
    pthread_create(&fifo_output_thread, NULL, open_output_fifo, NULL);
}

void* start_mplayer(void* playlist_loc_void){
    pthread_mutex_lock(&state_mutex);
    state.stopped = 0;
    stopped = 0;
    pthread_mutex_unlock(&state_mutex);

    //open_fifo(NULL);
    while(q_elt && !stopped){
        pthread_mutex_lock(&state_mutex);
        state.item = q_elt->ID;
        state.playlist_item_name = q_elt->name;
        state.pos = 0;
        long ns;
        long all;
        time_t sec;
        struct timespec spec;

        clock_gettime(CLOCK_REALTIME, &spec);
        sec = spec.tv_sec;
        ns = spec.tv_nsec;

        all = (long) sec * 10000000L + (long) ns/100;
        state.playback_start_ticks = all;
        pthread_mutex_unlock(&state_mutex);
        inform_progress_update(state);

        int len = 1;
        len += strlen("mplayer -quiet -slave -prefer-ipv4 -nolirc -cache 1024 -cache-min 80 -input file=\"\" \"\" > \"\" 2> /dev/null");
        len += strlen(q_elt->play_loc);
        len += strlen(MPLAYER_FIFO);
        len += strlen(MPLAYER_OUTPUT_FIFO);
        char * call_str = malloc(len * sizeof(char));
        strcpy(call_str, "mplayer -quiet -slave -prefer-ipv4 -nolirc -cache 1024 -cache-min 80 -input file=\"");
        strcat(call_str, MPLAYER_FIFO);
        strcat(call_str, "\" \"");
        strcat(call_str, q_elt->play_loc);
        strcat(call_str, "\" > \"");
        strcat(call_str, MPLAYER_OUTPUT_FIFO);
        strcat(call_str, "\" 2> /dev/null");
        
        //sleep(10);
        system(call_str);
        if(play_prev && !go_back){
            play_prev = 0;
        } else if(play_prev && q_elt->prev && go_back){
            q_elt = q_elt->prev;
            play_prev = 0;
            go_back = 0;
        } else if (play_prev){ //If on first song
            play_prev = 0;
            go_back = 0;
        } else if(stopped){
            q_elt = q.head;
        } else {
            q_elt = q_elt->next;
        }

        if(!q_elt && state.repeat){
            q_elt = q.head;
        }
    }
    pthread_mutex_lock(&run_mutex);
    mplayer_running = 0;
    pthread_mutex_unlock(&run_mutex);
    inform_stopped(state);
    return NULL;
}

void *open_control_fifo(void * v){
    mkfifo(MPLAYER_FIFO,S_IRWXU+S_IRWXG + S_IRWXO);
    mp_fifo = open(MPLAYER_FIFO, O_WRONLY);
    return NULL;
}

void *open_output_fifo(void * v){
    mkfifo(MPLAYER_OUTPUT_FIFO,S_IRWXU+S_IRWXG + S_IRWXO);
    mp_out_fifo = open(MPLAYER_OUTPUT_FIFO, O_RDONLY);
    fcntl(mp_out_fifo, F_SETFL, O_NONBLOCK);
    return NULL;
}

char *constr_http_loc(char *str, int start, int end){
    char * song_id = strndup(str + start, end - start);
    int len = snprintf(NULL, 0, MPW_HTTP_LOC_BASE, server_addr, song_id, user_id, device_id, acc_tok);
    char * http_loc = malloc((len + 1) * sizeof(char));
    sprintf(http_loc, MPW_HTTP_LOC_BASE, server_addr, song_id, user_id, device_id, acc_tok);
    free(song_id);

    return http_loc;
}




int play_playlist(char * str, jsmntok_t *t, int num_tok, int start_idx){ 
    pthread_mutex_lock(&run_mutex);
    if(mplayer_running){
        pthread_mutex_unlock(&run_mutex);
        stop();
    } else {
        pthread_mutex_unlock(&run_mutex);
    }
    MQ_empty(&q);
    int i;
    for(i = 0; i < num_tok; i++){
        if(t[i].type == JSMN_STRING && !strncmp(str + t[i].start, "ItemIds", 7)){
            int j;
            for(j = 0; j < t[i + 1].size; j++){
                char *song_id = malloc(t[i + j + 2].end - t[i + j + 2].start + 1);
                sprintf(song_id, "%.*s", t[i + j + 2].end - t[i + j + 2].start, str + t[i + j + 2].start);
                char* s = constr_http_loc(str, t[i + j + 2].start, t[i + j + 2].end);
                int len = snprintf(NULL, 0, "playlistitem%d", j);
                char* name = malloc((len + 1) * sizeof(char));
                sprintf(name, "playlistitem%d", j);
                MQ_add(&q, song_id, s, name);
            }
        }
    }

    pthread_mutex_lock(&state_mutex);

    state.vol = 100;
    state.pos = 0;
    state.muted = 0;
    state.paused = 0;
    state.q = q;
    state.stopped = 0;
    state.item = NULL;
    pthread_mutex_unlock(&state_mutex);
    q_elt = q.head;
    int idx = 0;
    
    while(idx < start_idx && q_elt->next){
        q_elt = q_elt->next;
        idx++;
    }
    pthread_mutex_lock(&run_mutex);
    mplayer_running = 1;
    pthread_mutex_unlock(&run_mutex);
    pthread_create(&mplayer_thread, NULL, start_mplayer, NULL);
    
    //Send initial playing info via curl.
    inform_initial_playing(q_elt->ID, q_elt->name, &q);
    
    pthread_create(&curl_prog_upd_thread, NULL, send_progress, (void *)&state);
    return 0;
}

int toggle_pause(){ 
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pause\n",6);
    pthread_mutex_lock(&state_mutex);
    state.paused = 1 - state.paused;
    pthread_mutex_unlock(&state_mutex);
    pthread_mutex_unlock(&fifo_control_mutex);
    inform_progress_update(state);
    return 0;
}

int next(){
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pausing_keep quit\n", 18);
    pthread_mutex_unlock(&fifo_control_mutex);
    //quit(); //Ends current song
    return 0;
}

int prev(){
    play_prev = 1;
    if(get_percent_pos() <= 5){
        go_back = 1;
    } else {
        go_back = 0;
    }
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pausing_keep quit\n", 18);
    pthread_mutex_unlock(&fifo_control_mutex);
    
    return 0;
}

int stop(){
    stopped = 1;
    pthread_mutex_lock(&state_mutex);
    state.stopped = 1;
    pthread_mutex_unlock(&state_mutex);
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pausing_keep quit\n", 18);
    pthread_mutex_unlock(&fifo_control_mutex);
    while(mplayer_running){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo,"pausing_keep quit\n", 18);
        pthread_mutex_unlock(&fifo_control_mutex);
        usleep(100000);
    }
    inform_stopped(state);
    return 0;
}

int quit(){
    stop();
    pthread_join(mplayer_thread, NULL);
    close(mp_fifo);
    return 0;
}

int flush_output_buffer(){
    pthread_mutex_lock(&fifo_output_mutex);
    char c;
    while(read(mp_out_fifo, &c, 1) > 0){
        ;//Do nothing
    }
    pthread_mutex_unlock(&fifo_output_mutex);
    return 0;

}

//pattern uses printf format, one %d allowed only (ends with \n) if successful, return 1
int match_line_int(char * pattern, int *d){
    char buf[1024]; //Assume all shorter than 1024;
    char c = '\0';
    char * cur = buf;
    pthread_mutex_lock(&fifo_output_mutex);
    int oldfl = fcntl(mp_out_fifo, F_GETFL);
    fcntl(mp_out_fifo, F_SETFL, oldfl & ~O_NONBLOCK);
    while(c != '\n' && read(mp_out_fifo, &c, 1) > 0){
        *cur = c;
        cur ++;
    }
    *cur = '\n';
    cur++;
    *cur = '\0';
    fcntl(mp_out_fifo, F_SETFL, O_NONBLOCK);
    pthread_mutex_unlock(&fifo_output_mutex);
    //printf("BLAH: %s\n", buf);
    return sscanf(buf, pattern, d);
}

//pattern uses printf format, one %d allowed only (ends with \n) if successful, return 1
double match_line_doub(char * pattern, double *d){
    char buf[1024]; //Assume all shorter than 1024;
    char c = '\0';
    char * cur = buf;
    pthread_mutex_lock(&fifo_output_mutex);
    int oldfl = fcntl(mp_out_fifo, F_GETFL);
    fcntl(mp_out_fifo, F_SETFL, oldfl & ~O_NONBLOCK);
    while(c != '\n' && read(mp_out_fifo, &c, 1) > 0){
        *cur = c;
        cur ++;
    }
    *cur = '\n';
    cur++;
    *cur = '\0';

    fcntl(mp_out_fifo, F_SETFL, O_NONBLOCK);
    pthread_mutex_unlock(&fifo_output_mutex);
    //printf("BLAH: %s\n", buf);
    return sscanf(buf, pattern, d);
}

int get_percent_pos(){
    pthread_mutex_lock(&read_prop_mutex);
    flush_output_buffer();
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, "pausing_keep get_percent_pos\n", 29);
    pthread_mutex_unlock(&fifo_control_mutex);
    int d = -1;
    while(!state.stopped && !match_line_int("ANS_PERCENT_POSITION=%d\n", &d)){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo, "pausing_keep get_percent_pos\n", 29);
        pthread_mutex_unlock(&fifo_control_mutex);
    }
    pthread_mutex_unlock(&read_prop_mutex);
    return d;
}

double get_time_pos(){
    pthread_mutex_lock(&read_prop_mutex);
    flush_output_buffer();
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, "pausing_keep_force get_time_pos\n", 32);
    pthread_mutex_unlock(&fifo_control_mutex);
    double d;
    while(!state.stopped && !match_line_doub("ANS_TIME_POSITION=%lf\n", &d)){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo, "pausing_keep_force get_time_pos\n", 32);
        pthread_mutex_unlock(&fifo_control_mutex);
    }
    pthread_mutex_unlock(&read_prop_mutex);
    pthread_mutex_lock(&state_mutex);
    state.pos = d * 10000000;
    pthread_mutex_unlock(&state_mutex);
    return d;
}

double get_vol_level(){
    pthread_mutex_lock(&read_prop_mutex);
    flush_output_buffer();
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, "pausing_keep get_property volume\n", 33);
    pthread_mutex_unlock(&fifo_control_mutex);
    double d;
    while(!state.stopped && !match_line_doub("ANS_volume=%lf\n", &d)){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo, "pausing_keep get_property volume\n", 33);
        pthread_mutex_unlock(&fifo_control_mutex);
    }
    pthread_mutex_unlock(&read_prop_mutex);
    pthread_mutex_lock(&state_mutex);
    state.vol = d;
    pthread_mutex_unlock(&state_mutex);
    return d;
}


void set_time_pos(double sec){
    pthread_mutex_lock(&fifo_control_mutex);
    int len = snprintf(NULL, 0, "pausing_keep set_property time_pos %f\n", sec);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, "pausing_keep set_property time_pos %f\n", sec);
    write(mp_fifo, s, strlen(s));
    free(s);
    pthread_mutex_unlock(&fifo_control_mutex);
    get_time_pos();
    inform_progress_update(state);
}

void set_vol_level(double lvl){
    if((lvl - state.vol) < .5 && (state.vol - lvl) < .5){
        return;
    }
    pthread_mutex_lock(&fifo_control_mutex);
    int len = snprintf(NULL, 0, "pausing_keep set_property volume %f\n", lvl);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, "pausing_keep set_property volume %f\n", lvl);
    write(mp_fifo, s, strlen(s));
    free(s);
    pthread_mutex_unlock(&fifo_control_mutex);
    pthread_mutex_lock(&state_mutex);
    state.vol = lvl;
    pthread_mutex_unlock(&state_mutex);
    //get_vol_level();
    inform_progress_update(state);
}

void toggle_mute(){
    pthread_mutex_lock(&fifo_control_mutex);
    pthread_mutex_lock(&state_mutex);
    state.muted = 1 - state.muted;
    pthread_mutex_unlock(&state_mutex);
    int len = snprintf(NULL, 0, "pausing_keep set_property mute %d\n", state.muted);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, "pausing_keep set_property mute %d\n", state.muted);
    write(mp_fifo, s, strlen(s));
    free(s);
    pthread_mutex_unlock(&fifo_control_mutex);
    inform_progress_update(state);
}

void set_repeat_all(){
    pthread_mutex_lock(&state_mutex);
    state.repeat = 1;
    pthread_mutex_unlock(&state_mutex);
}

void set_repeat_none(){
    pthread_mutex_lock(&state_mutex);
    state.repeat = 0;
    pthread_mutex_unlock(&state_mutex);
}
