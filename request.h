#ifndef __SOCKET_L__
#define __SOCKET_L__

#include "hashmap.h"

typedef struct SocketData {
	int sock;
	void *servinfo;
	char *HOST, *PORT;
} socket_t;

socket_t *get_socket(char *HOST, char *PORT);
int destroy_socket(socket_t *);

char *create_header(int STATUS, int *header_max, hashmap *status_code, hashmap *headers, int post_data_size, char *post_data);
hashmap *read_headers(char *header_str, void (*print_key)(void *), int *header_end);
// res is the response body, max_len is the length of the
// char ** returned. So an input of:
// ["10", "586", 20], would result in:
// char ** = ["10", "586", "20"], max_len = 3
char **handle_array(char *res, int *max_len);

// resize function (internal)
void *resize_array(void *arr, int *max_len, int curr_index, size_t singleton_size);

#endif