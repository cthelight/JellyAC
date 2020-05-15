#include "jellyac.h"
#include "websock.h"
#include <curl/curl.h>
#include <sys/time.h>
#include <time.h>

#define JELLYAC_CLIENT "JellyAC"
#define JELLYAC_DEVICE "JellyAC"
#define JELLYAC_VERSION "0.0.1"

#define JELLYAC_ACC_TOK_ID_STR "AccessToken"
#define JELLYAC_USR_ID_ID_STR "UserId"

const char * full_json_str = "{\"PlayableMediaTypes\":[\"Audio\"],\"SupportedCommands\":[\"VolumeUp\",\"VolumeDown\",\"Mute\",\"Unmute\",\"ToggleMute\",\"SetVolume\", \"SetRepeatMode\",\"PlayMediaSource\"],\"SupportsPersistentIdentifier\":false,\"SupportsMediaControl\":true,\"DeviceProfile\":{\"MaxStreamingBitrate\":120000000,\"MaxStaticBitrate\":100000000,\"MusicStreamingTranscodingBitrate\":192000,\"DirectPlayProfiles\":[{\"Container\":\"mp3\",\"Type\":\"Audio\",\"AudioCodec\":\"mp3\"},{\"Container\":\"flac\",\"Type\":\"Audio\"},{\"Container\":\"webma,webm\",\"Type\":\"Audio\"},{\"Container\":\"wav\",\"Type\":\"Audio\"},{\"Container\":\"ogg\",\"Type\":\"Audio\"}],\"TranscodingProfiles\":[{\"Container\":\"mp3\",\"Type\":\"Audio\",\"AudioCodec\":\"mp3\",\"Context\":\"Streaming\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"opus\",\"Type\":\"Audio\",\"AudioCodec\":\"opus\",\"Context\":\"Streaming\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"wav\",\"Type\":\"Audio\",\"AudioCodec\":\"wav\",\"Context\":\"Streaming\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"opus\",\"Type\":\"Audio\",\"AudioCodec\":\"opus\",\"Context\":\"Static\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"mp3\",\"Type\":\"Audio\",\"AudioCodec\":\"mp3\",\"Context\":\"Static\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"},{\"Container\":\"wav\",\"Type\":\"Audio\",\"AudioCodec\":\"wav\",\"Context\":\"Static\",\"Protocol\":\"http\",\"MaxAudioChannels\":\"2\"}],\"ContainerProfiles\":[],\"CodecProfiles\":[{\"Type\":\"VideoAudio\",\"Codec\":\"aac\",\"Conditions\":[{\"Condition\":\"NotEquals\",\"Property\":\"AudioProfile\",\"Value\":\"HE-AAC\"},{\"Condition\":\"Equals\",\"Property\":\"IsSecondaryAudio\",\"Value\":\"false\",\"IsRequired\":false}]},{\"Type\":\"VideoAudio\",\"Conditions\":[{\"Condition\":\"Equals\",\"Property\":\"IsSecondaryAudio\",\"Value\":\"false\",\"IsRequired\":false}]},{\"Type\":\"Video\",\"Codec\":\"h264\",\"Conditions\":[{\"Condition\":\"NotEquals\",\"Property\":\"IsAnamorphic\",\"Value\":\"true\",\"IsRequired\":false},{\"Condition\":\"EqualsAny\",\"Property\":\"VideoProfile\",\"Value\":\"high|main|baseline|constrained baseline\",\"IsRequired\":false},{\"Condition\":\"LessThanEqual\",\"Property\":\"VideoLevel\",\"Value\":\"42\",\"IsRequired\":false}]}],\"SubtitleProfiles\":[{\"Format\":\"vtt\",\"Method\":\"External\"}],\"ResponseProfiles\":[{\"Type\":\"Video\",\"Container\":\"m4v\",\"MimeType\":\"video/mp4\"}],\"MaxStaticMusicBitrate\":320000}}";

char* buf;
char* acc_tok = NULL;
char* user_id = NULL;
char device_id[128];
size_t write_data(void *buffer, size_t size, size_t nmmemb, void *userp){
    char *str_buf = (char *)buffer;

    char * temp_buf = malloc((nmmemb * sizeof(char)) + strlen(buf) + 1);
    strcpy(temp_buf, buf);
    strcat(temp_buf, str_buf);

    free(buf);
    buf = temp_buf;

    
    return size*nmmemb;
}

size_t write_data_temp(void *buffer, size_t size, size_t nmmemb, void *userp){
    printf("%s\n", (char *) buffer);

    
    return size*nmmemb;
}

int json_parse_id(){
    jsmn_parser p;
    jsmntok_t *t;
    jsmn_init(&p);
    int r = jsmn_parse(&p, buf, strlen(buf), NULL, 0);
    t = malloc(r * sizeof(jsmntok_t));
    jsmn_init(&p);
    r = jsmn_parse(&p, buf, strlen(buf), t, r);
    int i;
    for(i = 0; i < r; i++){
        if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, JELLYAC_ACC_TOK_ID_STR, t[i].end - t[i].start)){
            acc_tok = malloc((t[i + 1].end - t[i + 1].start + 1) * sizeof(char));
            acc_tok[t[i + 1].end - t[i + 1].start] = '\0';
            strncpy(acc_tok, buf + t[i + 1].start, t[i + 1].end - t[i + 1].start);
        } else if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, JELLYAC_USR_ID_ID_STR, t[i].end - t[i].start)){
            user_id = malloc((t[i + 1].end - t[i + 1].start + 1) * sizeof(char));
            user_id[t[i + 1].end - t[i + 1].start] = '\0';
            strncpy(user_id, buf + t[i + 1].start, t[i + 1].end - t[i + 1].start);
        }
    }
    free(t);
    return 1;
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
    printf("%s, %d\n", acc_tok, len);
    str = malloc(len * sizeof(char));
    int d = sprintf(str, "X-Emby-Authorization: MediaBrowser Client=\"%s\", Device=\"%s\", DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"", JELLYAC_CLIENT, JELLYAC_DEVICE, device_id, 
    JELLYAC_VERSION, acc_tok);
    //printf("%d\n", d);
    return str;
}

int initial_jellyfin_auth(){
    CURL *curl_auth, *curl_full;
    CURLcode res;

    //
    curl_auth = curl_easy_init();
    if(curl_auth){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        curl_easy_setopt(curl_auth, CURLOPT_URL, "http://192.168.1.15:8096/Users/authenticatebyname");
        curl_easy_setopt(curl_auth, CURLOPT_POSTFIELDS, "{\"Username\": \"jellyfin\", \"pw\": \"\"}");
        curl_easy_setopt(curl_auth, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl_auth, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_auth);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_auth);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
    }
    return 4;
}

int main(int argc, char const *argv[])
{
    //make buf a valid pointer for freeing
    buf = malloc(2 *sizeof(char));
    *buf = '\0';
    //generate "unique" device id
    struct timeval t;
    gettimeofday(&t, NULL);
    snprintf(device_id, 128, "%ld", time(NULL) * 1000000 +t.tv_usec);

    curl_global_init(CURL_GLOBAL_ALL);
    initial_jellyfin_auth();
    json_parse_id();
    printf("%s\n", acc_tok);
    printf("%s\n", user_id);
    printf("%s\n", device_id);

    CURL *curl_full = curl_easy_init();
    CURLcode res;
    if(curl_full){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str_tok();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        curl_easy_setopt(curl_full, CURLOPT_URL, "http://192.168.1.15:8096/Sessions/Capabilities/Full");
        curl_easy_setopt(curl_full, CURLOPT_POSTFIELDS, full_json_str);
        curl_easy_setopt(curl_full, CURLOPT_WRITEFUNCTION, write_data_temp);
        curl_easy_setopt(curl_full, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl_full);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl_full);
        curl_slist_free_all(chunk);
        free(x_emb_auth_str);
    }


    curl_global_cleanup();
    free(buf);
    init_ws_conn();
    return 0;
}
