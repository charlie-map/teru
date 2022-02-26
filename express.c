#include <stdio.h>
#include <stdlib.h>
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

#include "express.h"

#define MAXLINE 4096

typedef struct Listener {
	char *r_type; // "GET" / "POST" / etc.

	void (*handler)(req_t, res_t); // handles processing for the request
									   // made by user

	/* more to come! */
} listen_t;

listen_t *new_listener(char *r_type, void (*handler)(req_t, res_t)) {
	listen_t *r = malloc(sizeof(listen_t));

	r->r_type = r_type;
	r->handler = handler;

	return r;
}

void free_listen_t(void *l_t) {
	free((listen_t *) l_t);

	return;
}

void print_listen_t(void *l_t) {
	printf("Request type: %s ***", ((listen_t *) l_t)->r_type);
	printf("Handle location: %d\n", ((listen_t *) l_t)->handler);

	return;
}

void print_app_settings(void *app_set) {
	printf("Setting value: %s ***", (char *) app_set);

	return;
}


/* setup an initial service */
app express() {
	app *app_t = malloc(sizeof(app));

	app_t->my_ptr = app_t;

	app_t->routes = make__hashmap(1, print_listen_t, free_listen_t);
	app_t->app_settings = make__hashmap(0, print_app_settings, NULL);

	app_t->server_open = 1;

	return *app_t;
}

void *acceptor_function(void *app_ptr);

int app_listen(char *PORT, app app_t) {
	socket_t *socket = get_socket(NULL, PORT);

	if (listen(socket->sock, BACKLOG) == -1) {
		perror("listen error");
		return 1;
	}

	app_t.my_ptr->socket = socket;

	// start acceptor thread
	pthread_t accept_thread;
	int check = pthread_create(&accept_thread, NULL, &acceptor_function, app_t.my_ptr);

	printf("server go vroom\n");

	while(getchar() != '0');

	destroy_socket(socket);

	return 0;
}

/* ROUTE BUILDER -- POINTS TO GENERIC ROUTE BUILDER
	|__ the only reason for these functions is to build a slightly
		simpler interface on top of the library */
int build_new_route(app app_t, char *type, char *endpoint, void (*handler)(req_t, res_t)) {
	// check that the route doesn't exist (assumes the type matches)
	hashmap__response *routes = (hashmap__response *) get__hashmap(app_t.routes, endpoint);

	for (int check_route = 0; routes && check_route < routes->payload__length; check_route) {
		listen_t *r = (listen_t *) routes->payload[check_route];

		if (strcmp(type, r->r_type) == 0) { // found match
			// replace current handler with new handler
			printf("\033[0;31m");
			printf("\n** Replacing previous handler **\n");
			printf("\033[0;37m");
			r->handler = handler;

			return 0;
		}
	}

	// otherwise insert new listen_t
	listen_t *r = new_listener(type, handler);

	insert__hashmap(app_t.routes, endpoint, r, printCharKey, compareCharKey, NULL);

	return 0;
}

int app_get(app app_t, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app_t, "get", endpoint, handler);
}

int app_post(app app_t, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app_t, "post", endpoint, handler);
}

/* EXPRESS APP SETTINGS BUILDER */
void app_use(app app_t, char *route, char *descript) {
	// update route name to have a "use" at the beginning

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 'u'; new_route_name[1] = 's'; new_route_name[2] = 'e'; new_route_name[3] = '\0';
	strcat(new_route_name, route);

	insert__hashmap(app_t.app_settings, new_route_name, descript);

	return;
}

void app_set(app app_t, char *route, char *descript) {
	// update route name to have a "set" at the beginning

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 's'; new_route_name[1] = 'e'; new_route_name[2] = 't'; new_route_name[3] = '\0';
	strcat(new_route_name, route);

	insert__hashmap(app_t.app_settings, new_route_name, descript);

	return;
}


/* HEADER PARSER 
	This handles taking in a header like:

	GET / HTTP/1.1
	Host: localhost:8888
	User-Agent: Firefox/91.0
	Accept: text/html
	Connection: keep-alive

	and creating a generalized form using a hashmap so you can do:

	get__hashmap(header_map, "Host") and receive "localhost:8888" in return

	-- along with this portion, there is also a map for the query (if the request url
		is "/?name=Charlie"), which would look similar to the above:

		get__hashmap(query_map, "name") and receive "Charlie" in return
	-- also have a body (if the header has a body portion)
*/
void print_header(void *h) {
	printf("**** %s\n", (char *) h);
}

typedef struct HeaderMap {
	char *type; // request type
	char *url; // request url

	hashmap *meta_header_map;

	hashmap *query_map;
	hashmap *body_map;
} map_header_t;

/* assumes the form name=Charlie&age=18 etc. */
int read_query(char *query) {
	
}

int read_url(char *url, int *url_max, int url_index, char *header_str, map_header_t *mp_h) {
	int read_char = 0;

	int *query_key_max = malloc(sizeof(int)), query_key_index = 0,
		*query_value_max = malloc(sizeof(int)), query_value_index = 0;
	char *query_key = NULL, *query_value = NULL;
	int read_key = 1;

	while (header_str[read_char] != ' ') {
		// if query_key is set, need to read values into the query_hashmap
		if (query_key) {
			if (header_str[read_char] == '&') {
				insert__hashmap(mp_h->query_map, query_key, query_value, NULL, compareCharKey, deleteCharKey);

				*query_key_max = 8; *query_value_max = 8;
				query_key_index = 0; query_value_index = 0;

				query_key = malloc(sizeof(char) * *query_key_max);
				query_value = malloc(sizeof(char) * *query_value_max);

				read_key = 1;
			} else if (header_str[read_char] == '=')
				read_key = 0;
			else {
				if (read_key) {
					query_key[query_key_index++] = header_str[read_char];

					query_key = resize_array(query_key, query_key_max, query_key_index, sizeof(char));
				} else {
					query_value[query_value_index++] = header_str[read_char];

					query_value = resize_array(query_value, query_value_max, query_value_index, sizeof(char));
				}
			}
		}

		// update has query to true
		if (header_str[read_line] == '?') {
			has_query = 1;

			mp_h->query_map = make__hashmap(0, print_header, deleteCharKey);

			query_key = malloc(sizeof(char) * *query_key_max);
			query_value = malloc(sizeof(char) * *query_value_max);
		}

		url[url_index++] = header_str[read_line];

		url = resize_array(url, url_max, url_index, sizeof(char));
	}

	free(query_key_max);
	free(query_value_max);

	return url_index + 1;
}

map_header_t *read_headers(char *header_str, int header_length) {
	map_header_t *mp_h = malloc(sizeof(map_header_t));

	// start with first line (special):
	// GET / HTTP/1.1
	// get the request type and url
	int *type_max = malloc(sizeof(int)), type_index = 0; *type_max = 8;
	char *type = malloc(sizeof(char) * *type_max);
	int *url_max = malloc(sizeof(int)), url_index = 0; *url_max = 8;
	char *url = malloc(sizeof(char) * *url_max);
	int *http_status_max = malloc(sizeof(int)), http_status_index = 0; *http_status_max = 8;
	char *http_status = malloc(sizeof(char) * *http_status_max);
	char *http_key = malloc(sizeof(char) * 5);
	strcpy(http_key, "http");

	int read_line = 0, curr_step = 0, has_query = 0;
	// curr_step moves with which part we should add to:
	// 0 for the request type
	// 1 for the url type
	// 2 for the http_status

	// has_query enables the query hashmap
	while (header_str[read_line] != '\n') {
		if (curr_step == 0) {
			type[type_index++] = header_str[read_line];

			type = resize_array(type, type_max, type_index, sizeof(char));
		} else if (curr_step == 1) {
			read_line += read_url(url, url_max, url_index, header_str + sizeof(char) * read_line, mp_h);
		} else {
			http_status[http_status_index++] = header_str[read_line];

			http_status = resize_array(http_status, http_status_max, http_status_index, sizeof(char));
		}

		read_line++;
		curr_step += header_str[read_line] == ' ' ? 1 : 0;
	}

	mp_h->type = type;
	mp_h->url = url;

	read_line++;

	// read the headers:
	int *header_end_pos = malloc(sizeof(int)); // for then checking the body
	mp_h->meta_header_map = read_headers(header_str, print_header, header_end_pos);

	// if the type matches, read all of the 
}

typedef struct ConnectionHandle {
	int p_handle; // socket specific pointer

	app *app_t; // all the meta data
} ch_t;

/* LISTENER FUNCTIONS FOR SOCKET */
// Based on my trie-suggestor-app (https://github.com/charlie-map/trie-suggestorC/blob/main/server.c) and
// Beej Hall websockets (http://beej.us/guide/bgnet/html/)
void *connection(void *app_ptr) {
	printf("new connnection\n");

	int new_fd = ((ch_t *) app_ptr)->p_handle;
	app app_t = *((ch_t *) app_ptr)->app_t;
	printf("%d with server %d\n", new_fd, app_t.server_open);

	int recv_res = 1;
	char *buffer = malloc(sizeof(char) * MAXLINE);
	int buffer_len = MAXLINE;

	// make a continuous loop for new_fd while they are still alive
	// as well as checking that the server is running
	while ((recv_res = recv(new_fd, buffer, buffer_len, 0)) != 0 && app_t.server_open) {
		if (recv_res == -1) {
			perror("receive: ");
			continue;
		}

		// otherwise parse header data
		printf("%d\n %s\n", recv_res, buffer);
		read_headers(buffer, recv_res / sizeof(char));
	}

	// if the close occurs due to thread_status, send an error page
	if (!app_t.server_open);

	close(new_fd);
	free(buffer);

	pthread_t *retval;
	// close thread
	pthread_exit(retval);

	return NULL;
}

void *acceptor_function(void *app_ptr) {
	app app_t = *(app *) app_ptr;

	int sock_fd = app_t.socket->sock;
	int new_fd;
	char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;

	while (1) {
		sin_size = sizeof(their_addr);
		new_fd = accept(sock_fd, (struct sockaddr *) &their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		printf("new socket %d\n", new_fd);
		// at this point we can send the user into their own thread
		ch_t *new_thread_data = malloc(sizeof(ch_t));
		new_thread_data->p_handle = new_fd;
		new_thread_data->app_t = app_t.my_ptr;
		pthread_t socket;
		pthread_create(&socket, NULL, &connection, new_thread_data);
	}

	return NULL;
}