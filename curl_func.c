#include "curl_func.h"
#include <pthread.h>
#include <time.h>

#define CF_CAP_BASE "http://%s/Sessions/Capabilities/Full" //String is server
#define CF_INIT_PLAY_BASE "http://%s/Sessions/Playing" //String is server
#define CF_PROG_BASE "http://%s/Sessions/Playing/Progress" //String is server
#define CF_STOP_BASE "http://%s/Sessions/Playing/Stopped" //String is server
#define CF_AUTH_BASE "http://%s/Users/authenticatebyname" //String is server
#define CF_AUTH_POST_BASE "{\"Username\": \"%s\", \"pw\": \"%s\"}" //String is username, then password

extern char * user_id;
extern char * acc_tok;
extern char device_id[128];
extern char *buf;
extern const char *user;
extern const char *server_addr;
extern const char *pass;

extern pthread_mutex_t prog_mutex;

const char * full_json_str = "{\"PlayableMediaTypes\":[\"Audio\"],\"SupportedCommands\":[\"VolumeUp\",\"VolumeDown\",\"Mute\",\"Unmute\",\"ToggleMute\",\"SetVolume\", \"SetRepeatMode\",\"PlayMediaSource\"],\"SupportsPersistentIdentifier\":false,\"SupportsMediaControl\":true,\"DeviceProfile\":{\"MaxStreamingBitrate\":120000000,\"MaxStaticBitrate\":100000000,\"MusicStreamingTranscodingBitrate\":192000,\"DirectPlayProfiles\":[{\"Container\":\"mp3\",\"Type\":\"Audio\",\"AudioCodec\":\"mp3\"},{\"Container\":\"flac\",\"Type\":\"Audio\"},{\"Container\":\"webma,webm\",\"Type\":\"Audio\"},{\"Container\":\"wav\",\"Type\":\"Audio\"},{\"Container\":\"ogg\",\"Type\":\"Audio\"}],\"TranscodingProfiles\":[{\"Container\":\"mp3\",\"Type\":\"Audio\",\"AudioCodec\":\"mp3\",\"Context\":\"Streaming\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"opus\",\"Type\":\"Audio\",\"AudioCodec\":\"opus\",\"Context\":\"Streaming\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"wav\",\"Type\":\"Audio\",\"AudioCodec\":\"wav\",\"Context\":\"Streaming\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"opus\",\"Type\":\"Audio\",\"AudioCodec\":\"opus\",\"Context\":\"Static\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"mp3\",\"Type\":\"Audio\",\"AudioCodec\":\"mp3\",\"Context\":\"Static\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"wav\",\"Type\":\"Audio\",\"AudioCodec\":\"wav\",\"Context\":\"Static\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"}],\"ContainerProfiles\":[],\"CodecProfiles\":[{\"Type\":\"VideoAudio\",\"Codec\":\"aac\",\"Conditions\":[{\"Condition\":\"NotEquals\",\"Property\":\"AudioProfile\",\"Value\":\"HE-AAC\"},{\"Condition\":\"Equals\",\"Property\":\"IsSecondaryAudio\",\"Value\":\"false\",\"IsRequired\":false}]},{\"Type\":\"VideoAudio\",\"Conditions\":[{\"Condition\":\"Equals\",\"Property\":\"IsSecondaryAudio\",\"Value\":\"false\",\"IsRequired\":false}]},{\"Type\":\"Video\",\"Codec\":\"h264\",\"Conditions\":[{\"Condition\":\"NotEquals\",\"Property\":\"IsAnamorphic\",\"Value\":\"true\",\"IsRequired\":false},{\"Condition\":\"EqualsAny\",\"Property\":\"VideoProfile\",\"Value\":\"high|main|baseline|constrained baseline\",\"IsRequired\":false},{\"Condition\":\"LessThanEqual\",\"Property\":\"VideoLevel\",\"Value\":\"42\",\"IsRequired\":false}]}],\"SubtitleProfiles\":[{\"Format\":\"vtt\",\"Method\":\"External\"}],\"ResponseProfiles\":[{\"Type\":\"Video\",\"Container\":\"m4v\",\"MimeType\":\"video/mp4\"}],\"MaxStaticMusicBitrate\":320000}}";

char * constr_queue_json(MQ_t * q){
    //Does not include []
    int len = 1;
    MQ_elt_t *cur = q->head;
    while(cur){
        if(cur->prev) len += strlen(", ");
        len += strlen("{\"Id\": \"\",\"PlaylistItemId\":\"\"}");
        len += strlen(cur->ID);
        len += strlen(cur->name);
        cur = cur->next;
    }
    char * s = malloc(len * sizeof(char));
    *s = '\0'; //Allows strcat to work always
    cur = q->head;
    while(cur){
        if(cur->prev) strcat(s, ", ");
        strcat(s, "{\"Id\": \"");
        strcat(s, cur->ID);
        strcat(s,"\",\"PlaylistItemId\":\"");
        strcat(s, cur->name);
        strcat(s, "\"}");
        cur = cur->next;
    }
    return s;
}

char * constr_initial_playing(char *id, char *name, MQ_t *q){
    char * base = "{\"VolumeLevel\":100,\"IsMuted\":false,\"IsPaused\":false,\"RepeatMode\":\"RepeatNone\",\"MaxStreamingBitrate\":140000000,\"PositionTicks\":0,\"PlaybackStartTimeTicks\":%d,\"BufferedRanges\":[{\"start\":0,\"end\":12800000}],\"PlayMethod\":\"Transcode\",\"PlaySessionId\":\"%d\",\"PlaylistItemId\":\"%s\",\"MediaSourceId\":\"%s\",\"CanSeek\":true,\"ItemId\":\"%s\", \"NowPlayingQueue\":[%s]}";
    char *playing_arr = constr_queue_json(q);
    int tm = time(NULL);
    int len = snprintf(NULL, 0, base, tm, tm, name, id, id, playing_arr);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, base, tm, tm, name, id, id, playing_arr);
    free(playing_arr);
    return s;
}

char * constr_progress_update(mp_state_t state){
    char *paused_state;
    char *muted_state;
    char *repeat_state;
    if(state.paused){
        paused_state = "true";
    } else {
        paused_state = "false";
    }

    if(state.muted){
        muted_state = "true";
    } else {
        muted_state = "false";
    }

    if(state.repeat){
        repeat_state = "RepeatAll";
    } else {
        repeat_state = "RepeatNone";
    }

    char * base = "{\"VolumeLevel\":%.0f,\"IsMuted\":%s,\"IsPaused\":%s,\"RepeatMode\":\"%s\",\"MaxStreamingBitrate\":140000000,\"PositionTicks\":%.0f,\"PlaybackStartTimeTicks\":%d,\"BufferedRanges\":[{\"start\":0,\"end\":12800000}],\"PlayMethod\":\"Transcode\",\"PlaySessionId\":\"%d\",\"PlaylistItemId\":\"%s\",\"MediaSourceId\":\"%s\",\"CanSeek\":true,\"ItemId\":\"%s\", EventName: \"timeupdate\"}";
    //int tm = time(NULL);
    int len = snprintf(NULL, 0, base, state.vol, muted_state, paused_state, repeat_state, state.pos, state.playback_start_ticks, state.play_sess_id, state.playlist_item_name, state.item, state.item);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, base, state.vol, muted_state, paused_state, repeat_state, state.pos, state.playback_start_ticks, state.play_sess_id, state.playlist_item_name, state.item, state.item);
    return s;
}


char * constr_stopped(mp_state_t state){
    char *paused_state = "true";
    char *muted_state;
    char *repeat_state;

    if(state.muted){
        muted_state = "true";
    } else {
        muted_state = "false";
    }

    if(state.repeat){
        repeat_state = "RepeatAll";
    } else {
        repeat_state = "RepeatNone";
    }

    char * base = "{\"VolumeLevel\":%.0f,\"IsMuted\":%s,\"IsPaused\":%s,\"RepeatMode\":\"%s\",\"MaxStreamingBitrate\":140000000,\"PositionTicks\":%.0f,\"PlaybackStartTimeTicks\":%d,\"BufferedRanges\":[{\"start\":0,\"end\":12800000}],\"PlayMethod\":\"Transcode\",\"PlaySessionId\":\"%d\",\"PlaylistItemId\":\"%s\",\"MediaSourceId\":\"%s\",\"CanSeek\":true,\"ItemId\":\"%s\", EventName: \"timeupdate\"}";
    //int tm = time(NULL);
    int len = snprintf(NULL, 0, base, muted_state, paused_state, repeat_state, state.vol, state.pos, state.playback_start_ticks, state.play_sess_id, state.playlist_item_name, state.item, state.item);
    char * s = malloc((len + 1) * sizeof(char));
    sprintf(s, base, muted_state, paused_state, repeat_state, state.vol, state.pos, state.playback_start_ticks, state.play_sess_id, state.playlist_item_name, state.item, state.item);
    return s;
}

size_t write_data(void *buffer, size_t size, size_t nmmemb, void *userp){
    char *str_buf = (char *)buffer;

    char * temp_buf = malloc((nmmemb * sizeof(char)) + strlen(buf) + 1);
    strcpy(temp_buf, buf);
    strcat(temp_buf, str_buf);

    free(buf);
    buf = temp_buf;

    
    return size*nmmemb;
}

size_t dump_data(void *buffer, size_t size, size_t nmmemb, void *userp){
    //Dont care about response
    return size*nmmemb;
}

int inform_capability(){
    CURL *curl_full = curl_easy_init();
    CURLcode res;
    if(curl_full){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str_tok();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        //construct addr string
        char * ser_url;
        int len = snprintf(NULL, 0, CF_CAP_BASE, server_addr);
        ser_url = malloc((len + 1) * sizeof(char));
        sprintf(ser_url, CF_CAP_BASE, server_addr);
    
        curl_easy_setopt(curl_full, CURLOPT_URL, ser_url);
        curl_easy_setopt(curl_full, CURLOPT_POSTFIELDS, full_json_str);
        curl_easy_setopt(curl_full, CURLOPT_WRITEFUNCTION, dump_data);
        curl_easy_setopt(curl_full, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_full);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_full);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
        free(ser_url);
    }
    return 0;
}

int inform_initial_playing(char * id, char *name, MQ_t *q){
    CURL *curl_auth;
    CURLcode res;
    char * s = constr_initial_playing(id, name, q);

    curl_auth = curl_easy_init();
    if(curl_auth){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str_tok();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        //construct addr string
        char * ser_url;
        int len = snprintf(NULL, 0, CF_INIT_PLAY_BASE, server_addr);
        ser_url = malloc((len + 1) * sizeof(char));
        sprintf(ser_url, CF_INIT_PLAY_BASE, server_addr);

        curl_easy_setopt(curl_auth, CURLOPT_URL, ser_url);
        curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS, s);
        curl_easy_setopt(curl_auth, CURLOPT_WRITEFUNCTION, dump_data);
        curl_easy_setopt(curl_auth, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_auth);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_auth);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
        free(ser_url);
        
    }
    free(s);
    return 4;
}


int inform_progress_update(mp_state_t state){
    pthread_mutex_lock(&prog_mutex);
    CURL *curl_auth;
    CURLcode res;
    char * s = constr_progress_update(state);

    curl_auth = curl_easy_init();
    if(curl_auth){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str_tok();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        //construct addr string
        char * ser_url;
        int len = snprintf(NULL, 0, CF_PROG_BASE, server_addr);
        ser_url = malloc((len + 1) * sizeof(char));
        sprintf(ser_url, CF_PROG_BASE, server_addr);

        curl_easy_setopt(curl_auth, CURLOPT_URL, ser_url);
        curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS, s);
        curl_easy_setopt(curl_auth, CURLOPT_WRITEFUNCTION, dump_data);
        curl_easy_setopt(curl_auth, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_auth);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_auth);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
        free(ser_url);
    }
    free(s);
    pthread_mutex_unlock(&prog_mutex);
    return 4;
}


int inform_stopped(mp_state_t state){
    pthread_mutex_lock(&prog_mutex);
    CURL *curl_auth;
    CURLcode res;
    char * s = constr_progress_update(state);

    curl_auth = curl_easy_init();
    if(curl_auth){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str_tok();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        //construct addr string
        char * ser_url;
        int len = snprintf(NULL, 0, CF_STOP_BASE, server_addr);
        ser_url = malloc((len + 1) * sizeof(char));
        sprintf(ser_url, CF_STOP_BASE, server_addr);

        curl_easy_setopt(curl_auth, CURLOPT_URL, ser_url);
        curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS, s);
        curl_easy_setopt(curl_auth, CURLOPT_WRITEFUNCTION, dump_data);
        curl_easy_setopt(curl_auth, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_auth);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_auth);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
        free(ser_url);
    }
    free(s);
    pthread_mutex_unlock(&prog_mutex);
    return 4;
}



char * constr_x_emby_auth_str(){
    char* str;
    int len = 0;
    len += strlen(JELLYAC_CLIENT);
    len += strlen(JELLYAC_DEVICE);
    len += strlen(JELLYAC_VERSION);
    len += strlen(device_id);
    len += 82; //Rest of string
    str = malloc(len * sizeof(char));
    sprintf(str, "X-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\"", JELLYAC_CLIENT, JELLYAC_DEVICE, device_id, JELLYAC_VERSION);
    return str;
}

char * constr_x_emby_auth_str_tok(){
    char* str;
    int len = 0;
    len += strlen(JELLYAC_CLIENT);
    len += strlen(JELLYAC_DEVICE);
    len += strlen(JELLYAC_VERSION);
    len += strlen(device_id);
    len += strlen(acc_tok);
    len += 92; //Rest of string

    str = malloc(len * sizeof(char));
    sprintf(str, "X-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"", JELLYAC_CLIENT, JELLYAC_DEVICE, device_id, 
    JELLYAC_VERSION, acc_tok);

    return str;
}

int initial_jellyfin_auth(){
    CURL *curl_auth;
    CURLcode res;

    //
    curl_auth = curl_easy_init();
    if(curl_auth){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        //construct addr string
        char * ser_url;
        int len = snprintf(NULL, 0, CF_AUTH_BASE, server_addr);
        ser_url = malloc((len + 1) * sizeof(char));
        sprintf(ser_url, CF_AUTH_BASE, server_addr);

        curl_easy_setopt(curl_auth, CURLOPT_URL, ser_url);

        char * cred;
        int cred_len = snprintf(NULL, 0, CF_AUTH_POST_BASE, user, pass);
        cred = malloc((cred_len + 1) * sizeof(char));
        sprintf(cred, CF_AUTH_POST_BASE, user, pass);
        curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS, cred);
        curl_easy_setopt(curl_auth, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl_auth, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_auth);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_auth);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
        free(ser_url);
        free(cred);
    }
    return 4;
}

//Func to send progress periodically (in thread)
extern int mp_fifo;
void *send_progress(void *vd_state){
    mp_state_t * state = (mp_state_t *)vd_state;
    while(!state->stopped && (!(state->item) || !mp_fifo)){
        //until something is played, wait
        sleep(1);
    }
    while(!(state->stopped)){
        get_time_pos();
        inform_progress_update(*state);
        sleep(2);
    }
    return NULL;
}