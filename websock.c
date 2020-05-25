#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "mplayer_wrapper.h"

static struct lws *web_socket = NULL;

#define EXAMPLE_RX_BUFFER_BYTES (1024)

extern char* acc_tok;
extern char* user_id;
extern char device_id[128];

extern char ip[1024];
extern int port;

static int handle_callback( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	switch( reason )
	{
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lws_callback_on_writable( wsi );
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		;
			char * buf = (char *)in;
			jsmn_parser p;
			jsmntok_t *t;
			jsmn_init(&p);
			int r = jsmn_parse(&p, buf, strlen(buf), NULL, 0);
			t = malloc(r * sizeof(jsmntok_t));
			jsmn_init(&p);
			r = jsmn_parse(&p, buf, strlen(buf), t, r);
			int i;
			char play_state = 0;
			char gen_cmd = 0;
			for(i = 0; i < r; i++){
				if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "PlayCommand", 11)){
					if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "PlayNow", 7)){
						//First look for "StartIndex"
						int j;
						int d = 0;
						for(j = i + 2; j < r; j++){
							if(t[j].type = JSMN_STRING && !strncmp(buf + t[j].start, "StartIndex", 10)){
								char * s = strndup(buf + t[j + 1].start, t[j + 1].end - t[j + 1].start);
								d = atoi(s);
								free(s);
							}
						}
						play_playlist(buf, t, r, d);
					}
				} else if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "MessageType", 11)){
					if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "Playstate", 9)){
						play_state = 1;
					} else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "GeneralCommand", 14)){
						gen_cmd = 1;
					}
				}
			}

			for(i = 0; i < r; i++){
				if(play_state == 1 && t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "Command", 7)){
					if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "PlayPause", 9)){
						toggle_pause();
					} else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "NextTrack", 9)){
						next();
					} else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "PreviousTrack", 13)){
						prev();
					}  else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "Stop", 4)){
						stop();
					} else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "Seek", 4)){
						for(; i < r; i++){
							if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "SeekPositionTicks", 17)){
								char * s = strndup(buf + t[i + 1].start, t[i + 1].end - t[i + 1].start);
								double d = strtod(s, NULL);
								set_time_pos(d/10000000.0);
								free(s);
							}
						}
						//set_time_pos()
					}
				} else if(gen_cmd == 1 && t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "Name", 4)){
					if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "SetVolume", 9)){
						char args = 0;
						for(; i < r; i++){
							if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "Arguments", 9)){
								args = 1;
							} else if(args && t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "Volume", 6)){
								char * s = strndup(buf + t[i + 1].start, t[i + 1].end - t[i + 1].start);
								double d = strtod(s, NULL);
								set_vol_level(d);
								free(s);
							}
						}
					} else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "ToggleMute", 10)){
						toggle_mute();
					} else if(t[i + 1].type = JSMN_STRING && !strncmp(buf + t[i + 1].start, "SetRepeatMode", 13)){
						char args = 0;
						for(; i < r; i++){
							if(t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "Arguments", 9)){
								args = 1;
							} else if(args && t[i].type == JSMN_STRING && !strncmp(buf + t[i].start, "RepeatMode", 10)){
								if(t[i + 1].type == JSMN_STRING && !strncmp(buf + t[i + 1].start, "RepeatAll", 9)){
									set_repeat_all();
								} else if(t[i + 1].type == JSMN_STRING && !strncmp(buf + t[i + 1].start, "RepeatNone", 10)){
									set_repeat_none();
								} 
							}
						}
					}
				}
			}
			free(t);


            break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
            //printf("hello\n");
			unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
			unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
			size_t n = sprintf( (char *)p, "%u\n", rand() );
			lws_write( wsi, p, n, LWS_WRITE_TEXT );
			break;
		}

		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			web_socket = NULL;
			break;

		default:
			break;
	}

	return 0;
}

enum protocols
{
	PROTOCOL_EXAMPLE = 0,
	PROTOCOL_COUNT
};

static struct lws_protocols protocols[] =
{
	{
		"example-protocol",
		handle_callback,
		0,
		EXAMPLE_RX_BUFFER_BYTES,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

char * get_socket_path(){
	char * p;
	int len = 1; //account for null terminator
	len += strlen("/socket?api_key=&deviceId=");
	len += strlen(acc_tok);
	len += strlen(device_id);
	p = malloc(len * sizeof(char));
	strcpy(p, "/socket?api_key=");
	strcat(p, acc_tok);
	strcat(p, "&deviceId=");
	strcat(p, device_id);
	return p;
}

int init_ws_conn()
{
	lws_set_log_level(0, NULL);
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = 0;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	struct lws_context *context = lws_create_context( &info );
	
	time_t old = 0;
	while( 1 )
	{
		struct timeval tv;
		gettimeofday( &tv, NULL );

		/* Connect if we are not connected to the server. */
		if( !web_socket && tv.tv_sec != old )
		{
			struct lws_client_connect_info ccinfo = {0};
			ccinfo.context = context;
			ccinfo.address = ip;
			ccinfo.port = port;
			char * path = get_socket_path();
			ccinfo.path = path;
			ccinfo.host = lws_canonical_hostname( context );
			ccinfo.origin = "origin";
			ccinfo.protocol = protocols[PROTOCOL_EXAMPLE].name;
			web_socket = lws_client_connect_via_info(&ccinfo);
			//free(path);
		}

		// if( tv.tv_sec != old )
		// {
		// 	/* Send a random number to the server every second. */
		// 	lws_callback_on_writable( web_socket );
        //     //printf("hi\n");
		// 	old = tv.tv_sec;
		// }

		lws_service( context, /* timeout_ms = */ 250 );
	}

	lws_context_destroy( context );

	return 0;
}