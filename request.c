#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

// all socket related packages
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>

#include "stack.h"
#include "request.h"

#define MAXLINE 4096

void *resize_array(void *arr, int *max_len, int curr_index, size_t singleton_size) {
	while (curr_index >= *max_len) {
		*max_len *= 2;

		arr = realloc(arr, singleton_size * *max_len);
	}
	
	return arr;
}

void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {

	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

// param will be the formatter for an query paramaters:
// ?name=$&other_value=$
// expects an empty string if no query paramaters are wanted
char *build_url(char *request_url, int *req_length, char *query_param, char **attr_values) {
	int req_index = strlen(request_url);
	*req_length = req_index * (strlen(query_param) ? 2 : 1) + 1;
	char *full_request = malloc(sizeof(char) * *req_length);

	strcpy(full_request, request_url);

	int curr_attr_pos = 0;

	for (int check_param = 0; query_param[check_param]; check_param++) {

		if (query_param[check_param] == '$') {
			// read from arg list
			char *arg_value = attr_values[curr_attr_pos];
			int arg_len = strlen(arg_value);

			full_request = resize_array(full_request, req_length, req_index + arg_len, sizeof(char));
			strcpy(full_request + (sizeof(char) * req_index), arg_value);

			req_index += arg_len;
			curr_attr_pos++;
		} else
			full_request[req_index++] = query_param[check_param];

		full_request = resize_array(full_request, req_length, req_index, sizeof(char));

		full_request[req_index] = '\0';
	}

	*req_length = req_index;

	return full_request;
}

char *create_header(int STATUS, int *header_max, hashmap *status_code, hashmap *headers, int post_data_size, char *post_data) {
	int header_size;
	int header_index = 0; *header_max = 32;
	char *header = malloc(sizeof(char) * *header_max);

	char *status_char = malloc(sizeof(char) * 4);
	sprintf(status_char, "%d", STATUS);
	char *status_phrase = (char *) get__hashmap(status_code, status_char);
	int status_phrase_len = strlen(status_phrase);

	header_index = status_phrase_len + 6;
	header = resize_array(header, header_max, header_index, sizeof(char));
	sprintf(header, "%s %s\n", status_char, status_phrase);

	// read all content response headers
	int *key_num = malloc(sizeof(int));
	char **header_key = (char **) keys__hashmap(headers, key_num);

	for (int cp_header = 0; cp_header < *key_num; cp_header++) {
		char *header_value = (char *) get__hashmap(headers, header_key[cp_header]);

		header_index += strlen(header_key[cp_header]) + strlen(header_value) + 4;
		header = resize_array(header, header_max, header_index, sizeof(char));
		sprintf(header, "%s: %s\n", header_key[cp_header], header_value);

		header[header_index] = '\0';
	}

	free(key_num);

	int add_on = strlen(post_data) + 3;
	header = resize_array(header, header_max, header_index + add_on, sizeof(char));

	sprintf(header, "\n\n%s\0", post_data);

	*header_max = header_index + add_on;
	return header;
}

char **handle_array(char *res, int *max_len) {	
	int arr_index = 0;
	*max_len = 8;
	char **arr = malloc(sizeof(char *) * *max_len);

	int max_res_length = strlen(res);

	// create a stack that tells the "level of depth" within 
	// certain values within the array, so a comma in a string
	// will not count as being a separator in the array:
	// ["10, 2", 6]
	stack_tv2 *abstract_depth = stack_create();

	for (int curr_res_pos = 0; curr_res_pos < max_res_length; curr_res_pos++) {
		int *str_len = malloc(sizeof(int)), str_index = 0;
		*str_len = 8;
		char *str = malloc(sizeof(char) * *str_len);

		str[str_index] = '\0';

		// seek until ',' or ']' AND stack is empty
		while ((res[curr_res_pos] != ',' && res[curr_res_pos] != ']') || stack_size(abstract_depth)) {
			if (res[curr_res_pos] == '[' && !stack_size(abstract_depth)) {
				curr_res_pos++;
				continue;
			}

			if (res[curr_res_pos] == '"' && stack_size(abstract_depth) && res[curr_res_pos] == ((char *) stack_peek(abstract_depth))[0])
				stack_pop(abstract_depth);
			else if (res[curr_res_pos] == '"')
				stack_push(abstract_depth, "\"");
			else {
				str[str_index++] = res[curr_res_pos];

				str = (char *) resize_array(str, str_len, str_index, sizeof(char));
				str[str_index] = '\0';
			}
			curr_res_pos++;
		}

		free(str_len);

		arr[arr_index++] = str;
		arr = (char **) resize_array(arr, max_len, arr_index, sizeof(char *));
	}

	stack_destroy(abstract_depth);
	*max_len = arr_index;

	return arr;
}

hashmap *read_headers(char *header_str, void (*print_key)(void *), int *header_end) {
	int past_lines = 0;

	hashmap *header_map = make__hashmap(0, print_key, destroyCharKey);

	// jump past HTTP: status line
	while ((int) header_str[past_lines] != 10)
		past_lines++;

	past_lines += 1;

	// while the newline doesn't start with a newline (heh)
	// (double newline is end of header)
	while ((int) header_str[past_lines] != 10) {
		int *head_max = malloc(sizeof(int)), head_index = 0;
		*head_max = 8;
		char *head_tag = malloc(sizeof(char) * *head_max);

		int *attr_max = malloc(sizeof(int)), attr_index = 0;
		*attr_max = 8;
		char *attr_tag = malloc(sizeof(char) * *attr_max);

		// head head tag
		while ((int) header_str[past_lines + head_index] != 58) {

			head_tag[head_index++] = header_str[past_lines + head_index];

			head_tag = resize_array(head_tag, head_max, head_index, sizeof(char));
			head_tag[head_index] = '\0';
		}

		past_lines += head_index + 2;

		// read attr tag
		while ((int) header_str[past_lines + attr_index + 1] != 10) {
			attr_tag[attr_index++] = header_str[past_lines + attr_index];

			attr_tag = resize_array(attr_tag, attr_max, attr_index, sizeof(char));
			attr_tag[attr_index] = '\0';
		}

		// check for a carraige return (\r). This means the newline character is one further along
		past_lines += attr_index + ((int) header_str[past_lines + attr_index + 2] == 13 ? 3 : 2);

		insert__hashmap(header_map, head_tag, attr_tag, "", compareCharKey, destroyCharKey);

		free(head_max);
		free(attr_max);
	}

	*header_end = past_lines + 1;
	return header_map;
}

socket_t *get_socket(char *HOST, char *PORT) {
	// connection stuff
	//int *sock_fd = malloc(sizeof(int)); // listen on sock_fd
	int sock;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	struct sigaction sa;

	int yes = 1;
	int status;

	memset(&hints, 0, sizeof(hints)); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;	  // dont' care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;	  // fill in my IP for me

	if ((status = getaddrinfo(HOST, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes,
			sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
			close(sock);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(1);
	}

	socket_t *data_return = malloc(sizeof(socket_t));
	data_return->sock = sock;
	data_return->servinfo = servinfo;
	data_return->HOST = HOST;
	data_return->PORT = PORT;

	return data_return;
}

int destroy_socket(socket_t *socket_data) {
	if (!socket_data->servinfo)
		return 0;
	freeaddrinfo((struct addrinfo *) socket_data->servinfo);

	free(socket_data);

	return 0;
}

int spec_char_sum(char *request_structure, char search_char) {
	int sum = 0;
	// go through the char * and count the occurences of search_char
	for (int check_count = 0; request_structure[check_count]; check_count++) {
		if (request_structure[check_count] == search_char)
			sum++;
	}

	return sum;
}