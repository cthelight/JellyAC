test: websock.c jellyac.c mplayer_wrapper.c curl_func.c music_queue.c
	gcc websock.c jellyac.c mplayer_wrapper.c curl_func.c music_queue.c -o jac.out -lcurl -lwebsockets -lpthread
