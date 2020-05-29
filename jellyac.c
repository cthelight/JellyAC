#include "jellyac.h"
#include "websock.h"
#include "mplayer_wrapper.h"
#include <curl/curl.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define JAC_CONFIG_NAME ".jellyac.conf" 

char* buf;
char* acc_tok = NULL;
char* user_id = NULL;
const char* server_addr = NULL;
char device_id[128];
extern const char * full_json_str;
const char *user = NULL;
const char *pass = NULL;

char ip[1024]; //Assume shorter than 1024;
int port;

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



int main(int argc, char const *argv[])
{
    if(argc < 2){
        printf("Not Enough Arguments\n");
    }
    server_addr = argv[1];
    user = argv[2];
    if(argc >= 3){
        pass = argv[3];
    } else {
        //allow for empty password. MUST BE LAST ARGUMENT
        pass = "";
    }

    //Split address into ip and port
    char *c = (char *)server_addr;
    int idx = 0;
    while(*c != ':'){
        ip[idx] = *c;
        c++;
        idx++;
    }
    ip[idx] = '\0';
    c++;
    port = atoi(c);

    char *home = getenv("HOME");
    char * fname;
    int len = snprintf(NULL, 0, "%s/%s", home, JAC_CONFIG_NAME);
    fname = malloc((len + 1) * sizeof(char));
    sprintf(fname, "%s/%s", home, JAC_CONFIG_NAME);
    if(access(fname, F_OK) != -1){
        //Then file exists, so open and read
        int fd = open(fname, O_RDONLY);
        read(fd, device_id, 128);
        close(fd);
    } else {
        //generate "unique" device id
        struct timeval t;
        gettimeofday(&t, NULL);
        snprintf(device_id, 128, "%ld", time(NULL) * 1000000 +t.tv_usec);
        int fd = open(fname, O_WRONLY | O_CREAT, 0666);
        close(fd);
    }
    //make buf a valid pointer for freeing
    buf = malloc(2 *sizeof(char));
    *buf = '\0';
    curl_global_init(CURL_GLOBAL_ALL);
    initial_jellyfin_auth();
    json_parse_id();
 
    inform_capability();
    free(buf);
    mplayer_wrapper_init();
    init_ws_conn();
    curl_global_cleanup();
    return 0;
}
