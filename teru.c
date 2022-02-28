#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

#include "teru.h"

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
teru_t teru() {
	teru_t *app = malloc(sizeof(teru_t));

	app->app_ptr = app;

	app->status_code = make__hashmap(0, NULL, destroyCharKey);
	batchInsert__hashmap(app->status_code, "request_code.data");

	app->routes = make__hashmap(1, print_listen_t, free_listen_t);
	app->app_settings = make__hashmap(0, print_app_settings, destroyCharKey);

	app->server_active = 1;

	return *app;
}

void destroy_teru(teru_t *app) {
	deepdestroy__hashmap(app->status_code);
	deepdestroy__hashmap(app->routes);
	deepdestroy__hashmap(app->app_settings);

	free(app);

	return;
}

void *acceptor_function(void *app_ptr);

int app_listen(char *HOST, char *PORT, teru_t app) {
	socket_t *socket = get_socket(HOST, PORT);

	if (listen(socket->sock, BACKLOG) == -1) {
		perror("listen error");
		return 1;
	}

	app.app_ptr->socket = socket;

	// start acceptor thread
	pthread_t accept_thread;
	pthread_create(&accept_thread, NULL, &acceptor_function, app.app_ptr);

	printf("server go vroom\n");

	while (getchar() != '0');

	app.app_ptr->server_active = 0;
	shutdown(socket->sock, SHUT_RD);
	pthread_join(accept_thread, NULL);

	destroy_socket(socket);
	destroy_teru(app.app_ptr);

	return 0;
}

/* ROUTE BUILDER -- POINTS TO GENERIC ROUTE BUILDER
	|__ the only reason for these functions is to build a slightly
		simpler interface on top of the library */
int build_new_route(teru_t app, char *type, char *endpoint, void (*handler)(req_t, res_t)) {
	// check that the route doesn't exist (assumes the type matches)
	hashmap__response *routes = (hashmap__response *) get__hashmap(app.routes, endpoint);

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

	insert__hashmap(app.routes, endpoint, r, "", compareCharKey, NULL);

	return 0;
}

int app_get(teru_t app, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app, "GET", endpoint, handler);
}

int app_post(teru_t app, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app, "POST", endpoint, handler);
}

/* TERU APP SETTINGS BUILDER */
void app_use(teru_t app, char *route, ...) {
	// update route name to have a "set" at the beginning
	char *descript = NULL;

	va_list app_set_reader;
	va_start(app_set_reader, route);

	char *file_path = va_arg(app_set_reader, char *);
	char *sub_path = va_arg(app_set_reader, char *);

	descript = malloc(sizeof(char) * (strlen(file_path) + strlen(sub_path) + 1));
	strcpy(descript, file_path);
	strcat(descript, sub_path);

	// update route name to have a "use" at the beginning
	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 'u'; new_route_name[1] = 's'; new_route_name[2] = 'e'; new_route_name[3] = '\0';
	strcat(new_route_name, route);

	insert__hashmap(app.app_settings, new_route_name, descript, "", compareCharKey, destroyCharKey);

	return;
}

void app_set(teru_t app, char *route, ...) {
	// update route name to have a "set" at the beginning
	char *file_path = NULL;
	char *descript = NULL;

	va_list app_set_reader;
	va_start(app_set_reader, route);

	if (strcmp(route, "views") == 0) {
		file_path = va_arg(app_set_reader, char *);
		int fp_len = strlen(file_path);
		descript = malloc(sizeof(char) * (fp_len + 1));
		strcpy(descript, file_path);

		char *sub_path = va_arg(app_set_reader, char *);

		descript = realloc(descript, sizeof(char) * (fp_len + strlen(sub_path) + 1));
		strcat(descript, sub_path);
	}

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 's'; new_route_name[1] = 'e'; new_route_name[2] = 't'; new_route_name[3] = '\0';
	strcat(new_route_name, route);

	if (descript)
		insert__hashmap(app.app_settings, new_route_name, descript, "", compareCharKey, destroyCharKey);

	return;
}

/*
	options:
		-- SENDING DATA
			-t for simple text/plain -- expects a single char * (for data) in the overload
			-h for text/html; charset=UTF-8
				-- expects a char * (for data) and an int (length of char *) in the overload
			-hc for text/html; charset=?
				-- expects a char * (for data) and an int (length of char *)
					and a char * (for charset) in the overload
		-- ADDITIONAL HEADER OPTIONS:
			-o for adding another parameter
				-- expects a char * for the header name and a char *
					for the header value
*/
void data_send(int sock, hashmap *status_code, int status, char *options, ...) {
	// create initial hashmap
	hashmap *headers = make__hashmap(0, NULL, NULL);
	insert__hashmap(headers, "Access-Control-Allow-Origin", "*", "", compareCharKey, NULL);
	insert__hashmap(headers, "Connection", "Keep-Alive", "", compareCharKey, NULL);
	
	va_list read_opts;
	va_start(read_opts, options);

	char *data = NULL, *content_type = NULL;
	int data_length = 0;
	for (int check_option = 0; options[check_option]; check_option++) {
		if (options[check_option] != '-')
			continue;

		if (options[check_option + 1] == 't') {
			insert__hashmap(headers, "Content-Type", "text/plain", "", compareCharKey, NULL);

			data = va_arg(read_opts, char *);
			data_length = strlen(data) + 1;
		} else if (options[check_option + 1] == 'h') {
			// start with default values
			data = va_arg(read_opts, char *);
			data_length = va_arg(read_opts, int);
			char *html_option = va_arg(read_opts, char *);

			char *char_set = NULL;
			content_type = malloc(sizeof(char) * 25);
			strcpy(content_type, "text/html; charset=");

			if (options[check_option + 2] == 'c') {
				char_set = va_arg(read_opts, char *);

				content_type = realloc(content_type, sizeof(char) * (20 + strlen(char_set)));
				strcat(content_type, char_set);
			} else
				strcat(content_type, "UTF-8");

			insert__hashmap(headers, "Content-Type", content_type, "", compareCharKey, NULL);
		} else if (options[check_option + 1] == 'o') {
			char *new_header_name = va_arg(read_opts, char *);
			char *new_header_value = va_arg(read_opts, char *);

			insert__hashmap(headers, new_header_name, new_header_value, "", compareCharKey, NULL);
		}
	}

	char *lengthOf_data_length = NULL;
	if (data_length) {
		lengthOf_data_length = malloc(sizeof(char) * ((int) log10(data_length) + 2));
		sprintf(lengthOf_data_length, "%d", data_length);
		insert__hashmap(headers, "Content-Length", lengthOf_data_length, "", compareCharKey, NULL);
	}

	int *head_msg_len = malloc(sizeof(int));
	char *main_head_msg = create_header(status, head_msg_len, status_code, headers, data_length, data);

	int bytes_sent = 0;
	while ((bytes_sent = send(sock, main_head_msg, *head_msg_len - bytes_sent / sizeof(char), 0)) < sizeof(char) * *head_msg_len);

	free(head_msg_len);
	if (lengthOf_data_length) free(lengthOf_data_length);
	if (content_type) free(content_type);

	free(main_head_msg);
	deepdestroy__hashmap(headers);
	return;
}

void destroy_req_t(req_t *r) {
	free(r->type);
	free(r->url);
	free(r->http_stat);

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
	// I have no idea what these are anymore... just gonna comment em out
	// char *http_key = malloc(sizeof(char) * 5);
	// strcpy(http_key, "http");

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
	mp_h->http_stat = http_status;

	free(type_max);
	free(http_status_max);
	free(url_max);
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

	free(header_end_pos);
	return mp_h;
}

char *req_query(req_t req, char *name) {
	return (char *) get__hashmap(req.query_map, name);
}

char *req_body(req_t req, char *name) {
	return (char *) get__hashmap(req.body_map, name);
}

typedef struct ConnectionHandle {
	pthread_t *thread;

	int p_handle; // socket specific pointer
	int is_complete; // check for if the thread has finished running

	teru_t *app; // all the meta data

	struct ConnectionHandle *next; // keep track of current threads
} ch_t;

ch_t *build_new_thread(ch_t *head, int socket, teru_t *app, pthread_t *thread) {
	ch_t *new_thread = malloc(sizeof(ch_t));

	new_thread->thread = thread;

	new_thread->p_handle = socket;
	new_thread->is_complete = 0;

	new_thread->app = app;

	// splice in at position 1 (instead of linearly searching for end of list)
	ch_t *next = head->next;
	head->next = new_thread;
	new_thread->next = next;

	return new_thread;
}

/* Loop through current threads and if there is an is_complete true,
remove that thread */
int check_dead_threads(ch_t *curr) {
	ch_t *prev = curr;
	curr = curr->next;

	while (curr) {
		if (curr->is_complete == 1) {
			free(curr->thread);

			// extract node
			prev->next = curr->next;

			prev = curr;
			curr = curr->next;

			free(curr);

			continue;
		}

		prev = curr;
		curr = curr->next;
	}

	return 0;
}

int join_all_threads(ch_t *curr) {
	ch_t *prev = curr;
	curr = curr->next;

	while (curr) {
		if (curr->is_complete == 1) {
			free(curr->thread);

			// extract node
			prev->next = curr->next;
		} else {
			shutdown(curr->p_handle, SHUT_RD);
			pthread_join(*curr->thread, NULL);

			free(curr->thread);
		}

		prev = curr;
		curr = curr->next;

		free(prev);
	}

	return 0;
}

/* LISTENER FUNCTIONS FOR SOCKET */
// Based on my trie-suggestor-app (https://github.com/charlie-map/trie-suggestorC/blob/main/server.c) and
// Beej Hall websockets (http://beej.us/guide/bgnet/html/)
void *connection(void *app_ptr) {

	int new_fd = ((ch_t *) app_ptr)->p_handle;
	teru_t app = *((ch_t *) app_ptr)->app;

	int recv_res = 1;
	char *buffer = malloc(sizeof(char) * MAXLINE);
	int buffer_len = MAXLINE;

	// make a continuous loop for new_fd while they are still alive
	// as well as checking that the server is running
	while ((recv_res = recv(new_fd, buffer, buffer_len, 0)) != 0 && app.app_ptr->server_active) {
		if (recv_res == -1) {
			if (!app.app_ptr->server_active) break;

			perror("receive: ");
			continue;
		}

		// otherwise parse header data
		req_t *new_request = read_header_helper(buffer, recv_res / sizeof(char));

		// using the new_request, acceess the app to see how to handle it:
		hashmap__response *handler = get__hashmap(app.routes, new_request->url);
		if (!handler) { /* ERROR HANDLE */
			char *err_msg = malloc(sizeof(char) * (10 + strlen(new_request->type) + strlen(new_request->url)));
			sprintf(err_msg, "Cannot %s %s\n", new_request->type, new_request->url);
			data_send(new_fd, app.status_code, 404, "-t", err_msg);
			free(err_msg);

			destroy_req_t(new_request);
			free(handler);

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
			data_send(new_fd, app.status_code, 404, "-t", err_msg);
			free(err_msg);

			destroy_req_t(new_request);
			free(handler);

			break;
		}

		// find handle will now have the handler within it
		// can call the handler with the data
		res_t res = { .socket = new_fd, .status_code = app.status_code, 
					  .__dirname = (char *) get__hashmap(app.app_settings, "setviews") };
		((listen_t *) handler->payload[find_handle])->handler(*new_request, res);
	
		destroy_req_t(new_request);
		free(handler);
	}

	// if the close occurs due to thread_status, send an error page
	if (!app.app_ptr->server_active) {
		data_send(new_fd, app.status_code, 404, "-t", "Uh oh! The server is down... Try again in a bit.");
	}

	close(new_fd);
	free(buffer);

	return NULL;
}

void *acceptor_function(void *app_ptr) {
	teru_t app = *(teru_t *) app_ptr;

	int sock_fd = app.socket->sock;
	int new_fd;
	char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;

	ch_t *threads = malloc(sizeof(ch_t));
	threads->next = NULL;

	while (app.app_ptr->server_active) {
		// check threads for removal
		check_dead_threads(threads);

		sin_size = sizeof(their_addr);
		new_fd = accept(sock_fd, (struct sockaddr *) &their_addr, &sin_size);
		if (new_fd == -1) {
			if (!app.app_ptr->server_active) break;

			perror("accept");
			continue;
		}

		// at this point we can send the user into their own thread
		pthread_t *socket = malloc(sizeof(pthread_t));
		ch_t *new_thread = build_new_thread(threads, new_fd, app.app_ptr, socket);

		pthread_create(socket, NULL, &connection, new_thread);
	}

	// rejoin all active threads
	join_all_threads(threads);

	free(threads);

	return NULL;
}


/* RESULT SENDERS */
int res_sendFile(res_t res, char *name) {
	// load full file path
	int full_fpath_len = strlen(res.__dirname ? res.__dirname : getenv("PWD")) + strlen(name) + 1 + (!res.__dirname ? 1 : 0);
	char *full_fpath = malloc(sizeof(char) * full_fpath_len);
	strcpy(full_fpath, res.__dirname ? res.__dirname : getenv("PWD"));
	if (!res.__dirname)
		strcat(full_fpath, "/");
	strcat(full_fpath, name);

	FILE *f_pt = fopen(full_fpath, "r");

	if (!f_pt) {
		char *err_msg = malloc(sizeof(char) * (29 + full_fpath_len));
		sprintf(err_msg, "No such file or directory, \'%s\'", full_fpath);
		data_send(res.socket, res.status_code, 404, "-t", err_msg);
		free(err_msg);

		printf("\033[0;31m");
		printf("\n** No such file or directory, \'%s\' **\n", full_fpath);
		printf("\033[0;37m");

		free(full_fpath);
		return 1;
	}

	free(full_fpath);

	// if file opens, read from file to create a new request
	size_t read_line_size = sizeof(char) * 64;
	char *read_line = malloc(read_line_size);

	int *full_data_max = malloc(sizeof(int)), full_data_index = 0;
	*full_data_max = 64;
	char *full_data = malloc(sizeof(char) * *full_data_max);
	full_data[0] = '\0';

	int curr_line_len = 0;
	while ((curr_line_len = getline(&read_line, &read_line_size, f_pt)) != -1) {
		full_data_index += curr_line_len;
		full_data = resize_array(full_data, full_data_max, full_data_index + 1, sizeof(char));

		strcat(full_data, read_line);
		full_data[full_data_index] = '\0';
	}

	free(read_line);
	fclose(f_pt);

	data_send(res.socket, res.status_code, 200, "-h", full_data, full_data_index);
	
	free(full_data_max);
	free(full_data);
	return 0;
}

int res_end(res_t res, char *data) {
	data_send(res.socket, res.status_code, 200, "-t", data);
}