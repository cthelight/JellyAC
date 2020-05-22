#ifndef CURL_FUNC_H
#define CURL_FUNC_H
#include <curl/curl.h>
#include "music_queue.h"
#include "mplayer_wrapper.h"
#define JSMN_HEADER
#include "jellyac.h"
int initial_jellyfin_auth();
char * constr_x_emby_auth_str_tok();
char * constr_x_emby_auth_str();
int inform_initial_playing(char * id, char *name, MQ_t *q);
int inform_progress_update(mp_state_t state);
void *send_progress(void *vd_state);
int inform_stopped(mp_state_t state);
#endif