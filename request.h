#ifndef __SOCKET_L__
#define __SOCKET_L__

typedef struct SocketData {
	int sock;
	void *servinfo;
	char *HOST, *PORT;
} socket_t;

socket_t *get_socket(char *HOST, char *PORT);
int destroy_socket(socket_t *socket_data);

// res is the response body, max_len is the length of the
// char ** returned. So an input of:
// ["10", "586", 20], would result in:
// char ** = ["10", "586", "20"], max_len = 3
char **handle_array(char *res, int *max_len);

#endif