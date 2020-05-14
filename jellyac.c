#include "jellyac.h"
#include <curl/curl.h>
#include <sys/time.h>
#include <time.h>

#define JELLYAC_CLIENT "JellyAC"
#define JELLYAC_DEVICE "JellyAC"
#define JELLYAC_VERSION "10.5.5"

#define JELLYAC_ACC_TOK_ID_STR "AccessToken"
#define JELLYAC_USR_ID_ID_STR "UserId"

char* buf;
char* acc_tok = NULL;
char* user_id = NULL;
char device_id[128];
size_t write_data(void *buffer, size_t size, size_t nmmemb, void *userp){
    char *str_buf = (char *)buffer;
    //printf("%s\n", str_buf);

    char * temp_buf = malloc((nmmemb * sizeof(char)) + strlen(buf) + 1);
    // printf("%s\n\n", buf);
    // printf("%ld\n", (nmmemb * sizeof(char)) + strlen(buf) + 1);
    // printf("%ld\n", strlen(str_buf));
    strcpy(temp_buf, buf);
    strcat(temp_buf, str_buf);
    // printf("%s\n\n", buf);

    free(buf);
    buf = temp_buf;

    
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

int initial_jellyfin_auth(){
    CURL *curl;
    CURLcode res;

    //
    curl = curl_easy_init();
    if(curl){
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Accept:");
        char * x_emb_auth_str = constr_x_emby_auth_str();
        chunk = curl_slist_append(chunk, x_emb_auth_str);
        chunk = curl_slist_append(chunk, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.15:8096/Users/authenticatebyname");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"Username\": \"jellyfin\", \"pw\": \"\"}");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK){
            printf("ERROR\n");
        }
        curl_easy_cleanup(curl);
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
    curl_global_cleanup();
    free(buf);
    return 0;
}
