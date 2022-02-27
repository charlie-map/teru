#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

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
app *express() {
	app *app_t = malloc(sizeof(app));

	app_t->status_code = make__hashmap(0, NULL, destroyCharKey);
	batchInsert__hashmap(app_t->status_code, "request_code.data");

	app_t->routes = make__hashmap(1, print_listen_t, free_listen_t);
	app_t->app_settings = make__hashmap(0, print_app_settings, NULL);

	app_t->server_open = 1;

	return app_t;
}

void *acceptor_function(void *app_ptr);

int app_listen(char *HOST, char *PORT, app *app_t) {
	socket_t *socket = get_socket(HOST, PORT);

	if (listen(socket->sock, BACKLOG) == -1) {
		perror("listen error");
		return 1;
	}

	app_t->socket = socket;

	// start acceptor thread
	pthread_t accept_thread;
	int check = pthread_create(&accept_thread, NULL, &acceptor_function, app_t);

	printf("server go vroom\n");

	while(getchar() != '0');

	destroy_socket(socket);

	return 0;
}

/* ROUTE BUILDER -- POINTS TO GENERIC ROUTE BUILDER
	|__ the only reason for these functions is to build a slightly
		simpler interface on top of the library */
int build_new_route(app *app_t, char *type, char *endpoint, void (*handler)(req_t, res_t)) {
	// check that the route doesn't exist (assumes the type matches)
	hashmap__response *routes = (hashmap__response *) get__hashmap(app_t->routes, endpoint);

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

	insert__hashmap(app_t->routes, endpoint, r, "", compareCharKey, NULL);

	return 0;
}

int app_get(app *app_t, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app_t, "GET", endpoint, handler);
}

int app_post(app *app_t, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app_t, "POST", endpoint, handler);
}

/* EXPRESS APP SETTINGS BUILDER */
void app_use(app *app_t, char *route, char *descript) {
	// update route name to have a "use" at the beginning

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 'u'; new_route_name[1] = 's'; new_route_name[2] = 'e'; new_route_name[3] = '\0';
	strcat(new_route_name, route);

	insert__hashmap(app_t->app_settings, new_route_name, descript, "", compareCharKey, NULL);

	return;
}

void app_set(app *app_t, char *route, char *descript) {
	// update route name to have a "set" at the beginning

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 's'; new_route_name[1] = 'e'; new_route_name[2] = 't'; new_route_name[3] = '\0';
	strcat(new_route_name, route);

	insert__hashmap(app_t->app_settings, new_route_name, descript, "", compareCharKey, NULL);

	return;
}

void error_handle(hashmap *status_code, int sock, int status, char *err_msg) {
	int bytes_sent = 0;
	int err_msg_len = err_msg ? strlen(err_msg) : 0;
	hashmap *headers = make__hashmap(0, NULL, NULL);
	insert__hashmap(headers, "Access-Control-Allow-Origin", "*", "", compareCharKey, NULL);
	insert__hashmap(headers, "Connection", "Keep-Alive", "", compareCharKey, NULL);
	
	char *msg_len_len;
	if (err_msg_len) {
		insert__hashmap(headers, "Content-Type", "text/plain", "", compareCharKey, NULL);
		msg_len_len = malloc(sizeof(char) * ((int) log10(err_msg_len) + 2));
		sprintf(msg_len_len, "%d", err_msg_len);
		insert__hashmap(headers, "Content-Length", msg_len_len, "", compareCharKey, NULL);
	}

	int *head_msg_len = malloc(sizeof(int));
	char *err_head_msg = create_header(status, head_msg_len, status_code, headers, err_msg_len, err_msg);

	printf("%d %s\n", *head_msg_len, err_head_msg);
	while ((bytes_sent = send(sock, err_head_msg, *head_msg_len - bytes_sent / sizeof(char), 0)) < sizeof(char) * *head_msg_len) {
		printf("%d --", bytes_sent);
	}
	printf("%d\n", bytes_sent);

	if (err_msg_len) free(msg_len_len);
	free(headers);
	return;
}

void destroy_req_t(req_t *r) {
	free(r->type);
	free(r->url);

	if (r->meta_header_map)
		deepdestroy__hashmap(r->meta_header_map);

	if (r->query_map)
		deepdestroy__hashmap(r->query_map);
	if (r->body_map)
		deepdestroy__hashmap(r->body_map);

	free(r);

	return;
}

void destroy_res_t(res_t *r) {
	/* NEEDS UPDATING */
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

/* assumes the form name=Charlie&age=18&etc. */
int read_query(char *query, int curr_point, hashmap *header_ptr) {
	// setup key and value pointers
	int *query_key_max = malloc(sizeof(int)), query_key_index = 0,
		*query_value_max = malloc(sizeof(int)), query_value_index = 0;
	*query_key_max = 8;
	*query_value_max = 8;

	char *query_key = malloc(sizeof(char) * *query_key_max),
		 *query_value = malloc(sizeof(char) * *query_value_max);
	int read_key = 1;

	while (query[curr_point] != ' ') {
		if (query[curr_point] == '&') {
			insert__hashmap(header_ptr, query_key, query_value, "", compareCharKey, destroyCharKey);

			*query_key_max = 8; *query_value_max = 8;
			query_key_index = 0; query_value_index = 0;

			query_key = malloc(sizeof(char) * *query_key_max);
			query_value = malloc(sizeof(char) * *query_value_max);

			read_key = 1;
		}

		if (query[curr_point] == '=') {
			read_key = 0;
			curr_point++;
			continue;
		}

		if (read_key) {
			query_key[query_key_index++] = query[curr_point];

			query_key = resize_array(query_key, query_key_max, query_key_index + 1, sizeof(char));
			query_key[query_key_index] = '\0';
		} else {
			query_value[query_value_index++] = query[curr_point];

			query_value = resize_array(query_value, query_value_max, query_value_index + 1, sizeof(char));
			query_value[query_value_index] = '\0';
		}

		curr_point++;
	}

	if (query_key_index > 0 && query_value_index > 0)
		insert__hashmap(header_ptr, query_key, query_value, "", compareCharKey, destroyCharKey);

	free(query_key_max);
	free(query_value_max);

	return curr_point + 1;
}

int read_url(char **url, int *url_max, int url_index, char *header_str, req_t *mp_h) {
	int read_char = 0;

	while (header_str[read_char] != ' ') {
		// update has query to true
		if (header_str[read_char] == '?') {

			mp_h->query_map = make__hashmap(0, print_header, destroyCharKey);

			read_char += read_query(header_str + sizeof(char) * (read_char + 1), 0, mp_h->query_map);

			continue;
		}

		(*url)[url_index++] = header_str[read_char];

		*url = resize_array(*url, url_max, url_index + 1, sizeof(char));

		(*url)[url_index] = '\0';
		read_char++;
	}

	return url_index;
}

req_t *read_header_helper(char *header_str, int header_length) {
	req_t *mp_h = malloc(sizeof(req_t));

	mp_h->query_map = NULL;
	mp_h->body_map = NULL;

	// start with first line (special):
	// GET / HTTP/1.1
	// get the request type and url
	int *type_max = malloc(sizeof(int)), type_index = 0; *type_max = 8;
	char *type = malloc(sizeof(char) * *type_max);
	int *url_max = malloc(sizeof(int)), url_index = 0; *url_max = 8;
	char **url = malloc(sizeof(char *));
	*url = malloc(sizeof(char) * *url_max);
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

			type = resize_array(type, type_max, type_index + 1, sizeof(char));
			type[type_index] = '\0';
		} else if (curr_step == 1) {
			read_line += read_url(url, url_max, url_index, header_str + sizeof(char) * read_line, mp_h);
			curr_step++;
		} else {
			http_status[http_status_index++] = header_str[read_line];

			http_status = resize_array(http_status, http_status_max, http_status_index, sizeof(char));
		}

		read_line++;
		if (header_str[read_line] == ' ') {
			curr_step++;
			read_line++;
		}
	}

	mp_h->type = type;
	mp_h->url = *url;

	free(url);

	read_line++;

	// read the headers:
	int *header_end_pos = malloc(sizeof(int)); // for then checking the body
	mp_h->meta_header_map = read_headers(header_str, print_header, header_end_pos);

	// if the type matches (POST), read all the body
	if (strcmp(mp_h->type, "POST") == 0) {
		mp_h->body_map = make__hashmap(0, print_header, destroyCharKey);

		read_query(header_str + sizeof(char) * *header_end_pos, 0, mp_h->body_map);
	}

	return mp_h;
}

char *req_query(req_t req, char *name) {
	return (char *) get__hashmap(req.query_map, name);
}

char *req_body(req_t req, char *name) {
	return (char *) get__hashmap(req.body_map, name);
}

typedef struct ConnectionHandle {
	int p_handle; // socket specific pointer

	app *app_t; // all the meta data
} ch_t;

/* LISTENER FUNCTIONS FOR SOCKET */
// Based on my trie-suggestor-app (https://github.com/charlie-map/trie-suggestorC/blob/main/server.c) and
// Beej Hall websockets (http://beej.us/guide/bgnet/html/)
void *connection(void *app_ptr) {

	int new_fd = ((ch_t *) app_ptr)->p_handle;
	app *app_t = ((ch_t *) app_ptr)->app_t;
	printf("%d %d with server %d\n", new_fd, app_t, app_t->server_open);

	int recv_res = 1;
	char *buffer = malloc(sizeof(char) * MAXLINE);
	int buffer_len = MAXLINE;

	// make a continuous loop for new_fd while they are still alive
	// as well as checking that the server is running
	while ((recv_res = recv(new_fd, buffer, buffer_len, 0)) != 0 && app_t->server_open) {
		if (recv_res == -1) {
			perror("receive: ");
			continue;
		}

		// otherwise parse header data
		req_t *new_request = read_header_helper(buffer, recv_res / sizeof(char));

		// using the new_request, acceess the app to see how to handle it:
		printf("\nAPP ROUTE\n");
		hashmap__response *handler = get__hashmap(app_t->routes, "/");
		if (!handler) { /* ERROR HANDLE */
			char *err_msg = malloc(sizeof(char) * (10 + strlen(new_request->type) + strlen(new_request->url)));
			sprintf(err_msg, "Cannot %s %s\n", new_request->type, new_request->url);
			error_handle(app_t->status_code, new_fd, 404, err_msg);
			free(err_msg);

			destroy_req_t(new_request);

			break;
		}

		// there might be multiple (aka "/number" is a POST and a GET)
		// need to find the one that matches the request:
		int find_handle;
		for (find_handle = 0; find_handle < handler->payload__length; find_handle++) {
			if (strcmp(((listen_t *) handler->payload[find_handle])->r_type, new_request->type) == 0)
				break;
		}

		if (find_handle == handler->payload__length) { /* not found -- ERROR HANDLE */
			char *err_msg = malloc(sizeof(char) * (10 + strlen(new_request->type) + strlen(new_request->url)));
			sprintf(err_msg, "Cannot %s %s\n", new_request->type, new_request->url);
			error_handle(app_t->status_code, new_fd, 404, err_msg);
			free(err_msg);

			destroy_req_t(new_request);

			break;
		}

		// find handle will now have the handler within it
		// can call the handler with the data
		res_t res = { .socket = new_fd, .status_code = app_t->status_code, 
					  .__dirname = (char *) get__hashmap(app_t->app_settings, "setviews") };
		error_handle(app_t->status_code, new_fd, 200, "bigger and badder test\n");
		break;
		((listen_t *) handler->payload[find_handle])->handler(*new_request, res);
	}

	// if the close occurs due to thread_status, send an error page
	if (!app_t->server_open);

	close(new_fd);
	free(buffer);

	pthread_t *retval;
	// close thread
	pthread_exit(retval);

	return NULL;
}

void *acceptor_function(void *app_ptr) {
	app *app_t = (app *) app_ptr;

	int sock_fd = app_t->socket->sock;
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

		// at this point we can send the user into their own thread
		ch_t *new_thread_data = malloc(sizeof(ch_t));
		new_thread_data->p_handle = new_fd;
		new_thread_data->app_t = app_t;
		pthread_t socket;
		pthread_create(&socket, NULL, &connection, new_thread_data);
	}

	return NULL;
}


/* RESULT SENDERS */
int res_sendFile(res_t res, char *name) {
	printf("%d %s\n", res.socket, name);

	error_handle(res.status_code, res.socket, 200, "fck\n");

	return 0;

	int full_fpath_len = strlen(res.__dirname) + strlen(name) + 6;
	char *full_fpath = malloc(sizeof(char) * full_fpath_len);
	strcpy(full_fpath, res.__dirname);
	strcat(full_fpath, name);
	strcat(full_fpath, ".html");

	FILE *f_pt = fopen(full_fpath, "r");

	if (!f_pt) {
		// char *err_msg = malloc(sizeof(char) * (29 + full_fpath_len));
		// sprintf(err_msg, "No such file or directory, \'%s\'", full_fpath);
		error_handle(res.status_code, res.socket, 418, "fck\n");
		//free(err_msg);

		return 1;
	}

	// if file opens, read from file to create a new request
	return 0;
}

int res_end(res_t res, char *name) {

}