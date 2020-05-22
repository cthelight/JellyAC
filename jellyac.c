#include "jellyac.h"
#include "websock.h"
#include "mplayer_wrapper.h"
#include <curl/curl.h>
#include <sys/time.h>
#include <time.h>

char* buf;
char* acc_tok = NULL;
char* user_id = NULL;
char device_id[128];
extern const char * full_json_str;
size_t write_data_temp(void *buffer, size_t size, size_t nmmemb, void *userp){
    printf("%s\n", (char *) buffer);

    
    return size*nmmemb;
}

int json_parse_id(){
    printf("%s\n", buf);
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



int main(int argc, char const *argv[])
{
    mplayer_wrapper_init();
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
        curl_easy_setopt(curl_full, CURLOPT_URL, "http://192.168.1.3:8096/Sessions/Capabilities/Full");
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
    //start_mplayer(NULL);
    printf("HELLO\n");
    init_ws_conn();
    return 0;
}
