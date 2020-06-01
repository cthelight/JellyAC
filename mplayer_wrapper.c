#include "mplayer_wrapper.h"
#include "curl_func.h"
#include "music_queue.h"
#include <time.h>
#include <unistd.h>
#define MPLAYER_FIFO "/tmp/mplayer_input"
#define PLAYLIST_LOC "/tmp/playlist.txt"
#define MPLAYER_OUTPUT_FIFO "/tmp/mplayer_output"
//Server, songid, Userid, deviceid, API-key
#define MPW_HTTP_LOC_BASE "http://%s/Audio/%s/universal?UserId=%s&DeviceId=%s&MaxStreamingBitrate=140000000&Container=opus%%2Cmp3%%7Cmp3%%2Cflac%%2Cwebma%%2Cwebm%%2Cwav%%2Cogg&TranscodingContainer=ts&TranscodingProtocol=hls&AudioCodec=aac&api_key=%s"
#define MPW_CALL_BASE "mplayer -slave -idle -input file=\"%s\" -quiet -prefer-ipv4 -nolirc -msglevel statusline=6:global=6 -cache 1024 -cache-min 80 > \"%s\" 2> /dev/null"
pthread_t fifo_control_thread;
pthread_mutex_t fifo_control_mutex;
pthread_t mplayer_thread;
pthread_mutex_t run_mutex;
pthread_t mplayer_output_thread;

pthread_mutex_t play_mutex;

pthread_t curl_prog_upd_thread;
pthread_mutex_t state_mutex;
pthread_mutex_t upd_time_mutex;

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

mp_state_t state;

void * handle_mplayer_output(void *);
long get_time_ms();

/**
 * Launches an instance of mplayer in idle mode. Should be run in own thread, otherwise will never return (unless mplayer exits)
 */
void* start_mplayer(void* unused){
    int len = snprintf(NULL, 0, MPW_CALL_BASE, MPLAYER_FIFO, MPLAYER_OUTPUT_FIFO);
    char * call_str = malloc((len + 1) * sizeof(char));
    sprintf(call_str, MPW_CALL_BASE, MPLAYER_FIFO, MPLAYER_OUTPUT_FIFO);
    pthread_mutex_lock(&run_mutex);
    mplayer_running = 1;
    pthread_mutex_unlock(&run_mutex);
    system(call_str);
    free(call_str);
    pthread_mutex_lock(&run_mutex);
    mplayer_running = 0;
    pthread_mutex_unlock(&run_mutex);
    close(mp_fifo);
    return NULL;
}

/**
 * Initializes the mplayer instance.
 * Initializes all mutexes, opens the necessary fifos, and launches a new thread running an instance of mplayer
 */
void mplayer_wrapper_init(){
    MQ_init(&q);
    pthread_mutex_init(&run_mutex, NULL);
    pthread_mutex_init(&fifo_control_mutex, NULL);
    pthread_mutex_init(&read_prop_mutex, NULL);
    pthread_mutex_init(&state_mutex, NULL);
    pthread_mutex_init(&prog_mutex, NULL);
    pthread_mutex_init(&play_mutex, NULL);
    pthread_mutex_init(&upd_time_mutex, NULL);
    pthread_create(&mplayer_output_thread, NULL, handle_mplayer_output, NULL);
    mkfifo(MPLAYER_FIFO,S_IRWXU+S_IRWXG + S_IRWXO);
    pthread_create(&mplayer_thread, NULL, start_mplayer, NULL);
    mp_fifo = open(MPLAYER_FIFO, O_WRONLY);
}


/**
 * Given a string, and the start/end of the id field, return a malloc'd string containing the url pointing to the file on the server (with access token attached)
 */
char *constr_http_loc(char *str, int start, int end){
    char * song_id = strndup(str + start, end - start);
    int len = snprintf(NULL, 0, MPW_HTTP_LOC_BASE, server_addr, song_id, user_id, device_id, acc_tok);
    char * http_loc = malloc((len + 1) * sizeof(char));
    sprintf(http_loc, MPW_HTTP_LOC_BASE, server_addr, song_id, user_id, device_id, acc_tok);
    free(song_id);

    return http_loc;
}

/**
 * Tell mplayer to play the song that is at the current location in the queue (elt stored in q_elt).
 */
void play_queue_item(){
    pthread_mutex_lock(&fifo_control_mutex);
    int len = snprintf(NULL, 0, "loadfile \"%s\"\n", q_elt->play_loc);
    char *play_loc = malloc((len + 1) * sizeof(char));
    sprintf(play_loc, "loadfile \"%s\"\n", q_elt->play_loc);
    write(mp_fifo, play_loc, len);
    free(play_loc);
    pthread_mutex_unlock(&fifo_control_mutex);
    pthread_mutex_lock(&state_mutex);
    state.item = q_elt->ID;
    state.playlist_item_name = q_elt->name;
    state.pos = 0;
    
    state.playback_start_ticks = 10000000 * get_time_ms();
    pthread_mutex_unlock(&state_mutex);
    inform_progress_update(state);
}


/**
 * Takes a JSMN parsed string that contains song ids and looks throug it to construct a queue containing the songs in the specified order.
 * Also tells mplayer to start playing, while also launching a new thread that informs Jellyfin of status
 */
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
    //pthread_create(&mplayer_thread, NULL, start_mplayer, NULL);
    play_queue_item();
    
    //Send initial playing info via curl.
    inform_initial_playing(q_elt->ID, q_elt->name, &q);
    
    pthread_create(&curl_prog_upd_thread, NULL, send_progress, (void *)&state);
    return 0;
}

/**
 * Toggles the current pause state of mplayer
 */
int toggle_pause(){ 
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pause\n",6);
    pthread_mutex_unlock(&fifo_control_mutex);
    pthread_mutex_lock(&state_mutex);
    state.paused = 1 - state.paused;
    pthread_mutex_unlock(&state_mutex);
    inform_progress_update(state);
    return 0;
}

/**
 * Move to the next track in the queue. If no such item, stop playback.
 */
int next(){
    if(q_elt->next){
        q_elt = q_elt->next;
        play_queue_item();
    } else {
        stop();
    }
    
    return 0;
}

/**
 * Move to previous item in the queue. If >5% into song (or at head of queue), just restart current song.
 */
int prev(){
    if(get_percent_pos() <= 5 && q_elt->prev){
        q_elt = q_elt->prev;
    }
    play_queue_item();
    
    return 0;
}

/**
 * Stop the current mplayer instance. Goes back to idling
 */
int stop(){
    pthread_mutex_lock(&state_mutex);
    state.stopped = 1;
    pthread_mutex_unlock(&state_mutex);
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pausing_keep stop\n", 18);
    pthread_mutex_unlock(&fifo_control_mutex);
    inform_stopped(state);
    return 0;
}

/**
 * Quit the mplayer instance, and close the fifos
 */
int quit(){
    stop();
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo,"pausing_keep quit\n", 18);
    pthread_mutex_unlock(&fifo_control_mutex);
    pthread_join(mplayer_thread, NULL);
    close(mp_fifo);
    close(mp_out_fifo);
    return 0;
}

/**
 * Returns the current number of milliseconds since the linux epoch
 */
long get_time_ms(){
    struct timeval tm;

    gettimeofday(&tm, NULL);

    long secs = tm.tv_usec / 1000000 + tm.tv_sec;
    return secs;
}

/**
 * Opens the output fifo, and parses all output from mplayer.
 * Should be run in separate thread to allow consistent listening
 */
void *handle_mplayer_output(void * unused){
    mkfifo(MPLAYER_OUTPUT_FIFO,S_IRWXU+S_IRWXG + S_IRWXO);
    mp_out_fifo = open(MPLAYER_OUTPUT_FIFO, O_RDONLY);

    char buf[1024]; //Assume all shorter than 1024;
    char c = '\0';
    char * cur = buf;
    while(mplayer_running){
        cur = buf;
        c = '\0';
        while(c != '\n' && read(mp_out_fifo, &c, 1) > 0){
            *cur = c;
            cur ++;
        }
        *cur = '\n';
        cur++;
        *cur = '\0';
        int res_int;
        double res_doub;
        long time = get_time_ms();
        if(sscanf(buf, "ANS_PERCENT_POSITION=%d\n", &res_int) == 1){
            pthread_mutex_lock(&state_mutex);
            state.percent_pos = res_int;
            state.perc_pos_update_time = time;
            pthread_mutex_unlock(&state_mutex);
        } else if(sscanf(buf, "ANS_TIME_POSITION=%lf\n", &res_doub) == 1){
            pthread_mutex_lock(&state_mutex);
            state.pos = res_doub * 10000000;
            state.pos_update_time = time;
            pthread_mutex_unlock(&state_mutex);
        } else if(sscanf(buf, "ANS_volume=%lf\n", &res_doub) == 1){
            pthread_mutex_lock(&state_mutex);
            state.vol = res_doub;
            state.vol_update_time = time;
            pthread_mutex_unlock(&state_mutex);
        } else if(sscanf(buf, "EOF code: %d\n", &res_int) == 1 && res_int == 1){
            //File Ended, move to next song if possible
            pthread_mutex_lock(&state_mutex);
            if(q_elt->next){
                q_elt = q_elt->next;
                pthread_mutex_unlock(&state_mutex);
                play_queue_item();
            } else {
                pthread_mutex_unlock(&state_mutex);
                stop();
            }
        }
    }
    close(mp_out_fifo);
    return NULL;
}

/**
 * Returns the percentage value of the position of mplayer in the current song.
 */
int get_percent_pos(){
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, "pausing_keep get_percent_pos\n", 29);
    pthread_mutex_unlock(&fifo_control_mutex);
    int d = -1;
    long time = get_time_ms();
    while(!state.stopped && (state.perc_pos_update_time < time)){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo, "pausing_keep get_percent_pos\n", 29);
        pthread_mutex_unlock(&fifo_control_mutex);
        usleep(100000);
    }
    d = state.percent_pos;
    return d;
}

/**
 * Gets the actual location of mplayer in the current trach, in seconds.
 * Bypasses mutex lock. Only for use when setting position (bad hack but works)
 */
double get_time_pos_h(){
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, "pausing_keep_force get_time_pos\n", 32);
    pthread_mutex_unlock(&fifo_control_mutex);
    long time = get_time_ms();
    double d = -1;
    while(!state.stopped && (state.pos_update_time < time)){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo, "pausing_keep_force get_time_pos\n", 32);
        pthread_mutex_unlock(&fifo_control_mutex);
        usleep(100000);
    }
    d = state.pos;
    return d;
}

/**
 * Gets the actual location of mplayer in the current trach, in seconds.
 * Use this version when possible (unless deadlock is unavoidable otherwise)
 */
double get_time_pos(){
    pthread_mutex_lock(&upd_time_mutex);
    double d = get_time_pos_h();
    pthread_mutex_unlock(&upd_time_mutex);
    return d;
}

/**
 * Gets the current level of mplayer's volume
 */
double get_vol_level(){
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, "pausing_keep get_property volume\n", 33);
    pthread_mutex_unlock(&fifo_control_mutex);
    double d = -1;
    long time = get_time_ms();
    while(!state.stopped && (state.vol_update_time < time)){
        pthread_mutex_lock(&fifo_control_mutex);
        write(mp_fifo, "pausing_keep get_property volume\n", 33);
        pthread_mutex_unlock(&fifo_control_mutex);
        usleep(100000);
    }
    d = state.vol;
    return d;
}

/**
 * Sets the current playback location of mplayer to "sec" seconds into the song
 */
void set_time_pos(double sec){
    pthread_mutex_lock(&upd_time_mutex);
    state.pos = sec * 10000000;
    inform_progress_update(state);
    int len = snprintf(NULL, 0, "pausing_keep set_property time_pos %f\n", sec);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, "pausing_keep set_property time_pos %f\n", sec);
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, s, strlen(s));
    pthread_mutex_unlock(&fifo_control_mutex);
    free(s);
    double c;
    //Wait until reported time is within 2sec of expected (avoids lag in update on remote control)
    while(!state.stopped && !((c = get_time_pos_h()) - sec * 10000000 < 20000000 && sec * 10000000 - c < 20000000)){
        usleep(100000);
    }
    pthread_mutex_unlock(&upd_time_mutex);
    get_time_pos();
    inform_progress_update(state);
}

/**
 * Sets the volume level of the mplayer output to "lvl"
 */
void set_vol_level(double lvl){
    if((lvl - state.vol) < .5 && (state.vol - lvl) < .5){
        return;
    }
    pthread_mutex_lock(&state_mutex);
    state.vol = lvl;
    pthread_mutex_unlock(&state_mutex);
    inform_progress_update(state);
    int len = snprintf(NULL, 0, "pausing_keep set_property volume %f\n", lvl);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, "pausing_keep set_property volume %f\n", lvl);
    pthread_mutex_lock(&fifo_control_mutex);
    pthread_mutex_unlock(&fifo_control_mutex);
    write(mp_fifo, s, strlen(s));
    free(s);
    
}

/**
 * Toggle the mute state of mplayer
 */
void toggle_mute(){
    pthread_mutex_lock(&state_mutex);
    state.muted = 1 - state.muted;
    pthread_mutex_unlock(&state_mutex);
    int len = snprintf(NULL, 0, "pausing_keep set_property mute %d\n", state.muted);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, "pausing_keep set_property mute %d\n", state.muted);
    pthread_mutex_lock(&fifo_control_mutex);
    write(mp_fifo, s, strlen(s));
    pthread_mutex_unlock(&fifo_control_mutex);
    free(s);
    inform_progress_update(state);
}

/**
 * Set the repeat state to "all"
 */
void set_repeat_all(){
    pthread_mutex_lock(&state_mutex);
    state.repeat = 1;
    pthread_mutex_unlock(&state_mutex);
}

/**
 * Set the repeat state to "None"
 */
void set_repeat_none(){
    pthread_mutex_lock(&state_mutex);
    state.repeat = 0;
    pthread_mutex_unlock(&state_mutex);
}
