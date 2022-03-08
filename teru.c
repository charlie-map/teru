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
#include "typeinfer.h"

#define MAXLINE 4096

typedef struct Listener {
	char *r_type; // "GET" / "POST" / etc.
				  // "TERU_PUBLIC" for app_use()
	char *url_wrap; // for app_use() only
	int add_num; // for calculating which values to look at first (PQ-esque)

	void (*handler)(req_t, res_t); // handles processing for the request
								   // made by user

	/* more to come! */
} listen_t;

listen_t *new_listener(char *r_type, void (*handler)(req_t, res_t), int add_num, char *url_wrap) {
	listen_t *r = malloc(sizeof(listen_t));

	r->r_type = r_type;
	r->add_num = add_num;

	r->url_wrap = url_wrap;

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

	app->curr_add_num = 0;
	app->routes = make__hashmap(1, print_listen_t, free_listen_t);
	app->use_settings = make__hashmap(0, print_app_settings, destroyCharKey);
	app->set_settings = make__hashmap(0, print_app_settings, destroyCharKey);

	app->server_active = 1;

	return *app;
}

void destroy_teru(teru_t *app) {
	deepdestroy__hashmap(app->status_code);
	deepdestroy__hashmap(app->routes);

	deepdestroy__hashmap(app->use_settings);
	deepdestroy__hashmap(app->set_settings);

	free(app);

	return;
}

hashmap *inferer_map;

void *acceptor_function(void *app_ptr);
int fsck_directory(char *major_path, char *minor_fp); // check for if a file exists

int app_listen(char *HOST, char *PORT, teru_t app) {
	printf("\033[0;32m");
	printf("\nTeru Server starting on ");
	printf("\033[0;37m");
	printf("%s", HOST);
	printf("\033[0;32m");
	printf(":");
	printf("\033[0;37m");
	printf("%s", PORT);
	printf("\033[0;32m");
	printf("...\n\n");

	socket_t *socket = get_socket(HOST, PORT);
	inferer_map = infer_load();

	if (listen(socket->sock, BACKLOG) == -1) {
		perror("listen error");
		return 1;
	}

	app.app_ptr->socket = socket;

	// start acceptor thread
	pthread_t accept_thread;
	pthread_create(&accept_thread, NULL, &acceptor_function, app.app_ptr);

	printf("server go vroom\n");
	printf("\033[0;37m");

	while (getchar() != '0');

	app.app_ptr->server_active = 0;
	shutdown(socket->sock, SHUT_RD);
	pthread_join(accept_thread, NULL);

	destroy_socket(socket);
	destroy_teru(app.app_ptr);

	deepdestroy__hashmap(inferer_map);

	return 0;
}

/* ROUTE BUILDER -- POINTS TO GENERIC ROUTE BUILDER
	|__ the only reason for these functions is to build a slightly
		simpler interface on top of the library */
int build_new_route(teru_t app, char *type, char *endpoint, void (*handler)(req_t, res_t), char *url_wrap) {
	// check that the route doesn't exist (assumes the type matches)
	hashmap__response *routes = (hashmap__response *) get__hashmap(app.routes, endpoint, "");

	for (hashmap__response *rt_thru = routes; rt_thru; rt_thru = rt_thru->next) {
		listen_t *r = (listen_t *) rt_thru->payload;

		if (strcmp(type, r->r_type) == 0) { // found match
			// replace current handler with new handler
			printf("\033[0;31m");
			printf("\n** Replacing previous handler **\n");
			printf("\033[0;37m");
			r->handler = handler;

			return 0;
		}
	}

	if (routes)
		free(routes);

	// otherwise insert new listen_t
	listen_t *r = new_listener(type, handler, app.app_ptr->curr_add_num, url_wrap);
	app.app_ptr->curr_add_num += 1;

	insert__hashmap(app.routes, endpoint, r, "", compareCharKey, NULL);

	return 0;
}

int app_get(teru_t app, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app, "GET", endpoint, handler, NULL);
}

int app_post(teru_t app, char *endpoint, void (*handler)(req_t, res_t)) {
	return build_new_route(app, "POST", endpoint, handler, NULL);
}

/* TERU APP SETTINGS BUILDER */
void get_public_file(req_t req, res_t res) {
	// req for specific file name
	// "/public/style.css" for example
	res_sendFile(res, req.url);

	return;
}

/* UPDATE:
	if route starts with a "/", the it assumes setting of a public directory
*/
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

	insert__hashmap(app.use_settings, route, descript, "", compareCharKey, NULL);

	if (route[0] == '/') {
		// add if a new route
		build_new_route(app, "TERU_PUBLIC", route, get_public_file, route);
	}

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
		insert__hashmap(app.set_settings, new_route_name, descript, "", compareCharKey, destroyCharKey);

	return;
}

/*
	options:
		-- SENDING DATA
			-t for simple text/plain -- expects a single char * (for data) in the overload
			-i for infering the text type (simple function currently)
				-- expects a char * (for FILENAME), a char * (for data) and an int (length of char *)
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
		} else if (options[check_option + 1] == 'i') {
			char *file_data_name = va_arg(read_opts, char *);
			data = va_arg(read_opts, char *);
			data_length = va_arg(read_opts, int) + 1;

			char *content_type = content_type_infer(inferer_map, file_data_name, data, data_length);

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
	while ((bytes_sent = send(sock, main_head_msg + bytes_sent, *head_msg_len - bytes_sent / sizeof(char), 0)) < sizeof(char) * *head_msg_len);

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
	free(r);

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
	return (char *) get__hashmap(req.query_map, name, "");
}

char *req_body(req_t req, char *name) {
	return (char *) get__hashmap(req.body_map, name, "");
}

res_t *create_response_struct(int socket, hashmap *status_code, char *__dirname) {
	res_t *res = malloc(sizeof(res_t));

	res->res_self = res;

	res->render = 0;
	res->fileName = NULL; res->matchStart = NULL; res->matchEnd = NULL;
	res->render_matches = NULL;

	res->socket = socket;
	res->status_code = status_code;

	res->__dirname = __dirname;

	return res;
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

int match_hashmap_substrings(void *other_key, void *curr_key) {
	char *io_key = (char *) other_key, *ic_key = (char *) curr_key;

	// compare using length of ic_key
	int ic_key_len = strlen(ic_key), io_key_len = strlen(io_key);
	int check_length = io_key_len < ic_key_len ? io_key_len : ic_key_len;

	int check_match;
	for (check_match = 0; io_key[check_match] && check_match < check_length; check_match++) {
		if (io_key[check_match] != ic_key[check_match])
			break;
	}

	return check_match == check_length;
}

int is_lower_hashmap_data(void *key1, void *key2) {
	return ((listen_t *) key1)->add_num < ((listen_t *) key2)->add_num;
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
		int request_url_len = strlen(new_request->url);

		// using the new_request, acceess the app to see how to handle it:
		hashmap__response *handler;
		if (new_request->url[request_url_len - 1] == '/') // is not a file -- don't look for public directories
			handler = get__hashmap(app.routes, new_request->url, "w", is_lower_hashmap_data);
		else
			handler = get__hashmap(app.routes, new_request->url, "iw", match_hashmap_substrings, is_lower_hashmap_data);
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
		hashmap__response *reader;
		int is_public = 0;
		for (reader = handler; reader; reader = reader->next) { // nice
			// case: if r_type == "TERU_PUBLIC", check system directory to
			// see if there is a file in that folder that corresponds with
			// the name of the request
			if (!(new_request->url[request_url_len - 1] == '/') && strcmp(((listen_t *) reader->payload)->r_type, "TERU_PUBLIC") == 0) {
				if (fsck_directory((char *) get__hashmap(app.use_settings, ((listen_t *) reader->payload)->url_wrap, new_request->url), new_request->url)) {
					is_public = 1;
					break;
				}
			}

			if (strcmp(((listen_t *) reader->payload)->r_type, new_request->type) == 0)
				break;
		}

		if (!reader) { /* not found -- ERROR HANDLE */
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
		res_t *res = create_response_struct(new_fd, app.status_code,
			(char *) (is_public ? get__hashmap(app.use_settings, ((listen_t *) reader->payload)->url_wrap, "") : get__hashmap(app.set_settings, "setviews", "")));
		((listen_t *) reader->payload)->handler(*new_request, *res);

		destroy_req_t(new_request);
		destroy_res_t(res);
		clear__hashmap__response(handler);
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


typedef struct RenderSchema {
	int *max_render_match_check, render_match_check_index,
		render_start_matcher_index, render_end_matcher_index;

	int render_start_matcher_length, render_end_matcher_length;

	int *max_render_match_buffer, render_match_buffer_index;
	char *render_match_buffer;

	char *render_match_check;
	char *render_start_matcher, *render_end_matcher;

	hashmap *res_render_matches;
} render_scheme_t;
render_scheme_t *create_render_scheme(res_t *res) {
	render_scheme_t *new_scheme = malloc(sizeof(render_scheme_t));

	new_scheme->max_render_match_check = malloc(sizeof(int));
	*new_scheme->max_render_match_check = 8;
	new_scheme->render_match_check_index = 0;
	new_scheme->render_start_matcher_index = 0;
	new_scheme->render_end_matcher_index = 0;

	new_scheme->max_render_match_buffer = malloc(sizeof(int));
	*new_scheme->max_render_match_buffer = 8;
	new_scheme->render_match_buffer_index = 0;

	new_scheme->render_start_matcher_length = strlen(res->matchStart);
	new_scheme->render_end_matcher_length = strlen(res->matchEnd);

	new_scheme->res_render_matches = res->render_matches;

	new_scheme->render_match_buffer = malloc(sizeof(char) * 8);
	new_scheme->render_match_check = malloc(sizeof(char) * 8);
	new_scheme->render_start_matcher = res->matchStart;
	new_scheme->render_end_matcher = res->matchEnd;

	return new_scheme;
}

int scheme_reset(render_scheme_t *r_scheme) {
	r_scheme->render_match_check_index = 0;
	r_scheme->render_start_matcher_index = 0;
	r_scheme->render_end_matcher_index = 0;

	r_scheme->render_match_check[0] = '\0';

	return 0;
}

int destroy_render_scheme(render_scheme_t *r_scheme) {
	free(r_scheme->max_render_match_check);
	free(r_scheme->max_render_match_buffer);

	free(r_scheme->render_match_check);
	free(r_scheme->render_match_buffer);

	free(r_scheme);

	return 0;
}

int check_renders(render_scheme_t *r_scheme, char *full_line, char **full_data,
	int *full_data_max, int full_data_index) {
	
	int has_found_start_match; // checking if the matcher indices have changed
	int has_index_change; // for checking if data in buffer needs
							  // to be written to full_data
	// read through the line and check against render matches and see if we should start reading a key
	for (int read_full_line = 0; full_line[read_full_line]; read_full_line++) {
		has_index_change = 0;
		has_found_start_match = 0;
		// casing:
		// no nothing, look for render_start_matcher
		if (r_scheme->render_start_matcher_index < r_scheme->render_start_matcher_length) {
			// check for if line matches renderer:
			if (full_line[read_full_line] == r_scheme->render_start_matcher[r_scheme->render_start_matcher_index]) {
				has_found_start_match = 1;
				r_scheme->render_start_matcher_index++;

				if (r_scheme->render_start_matcher_index == r_scheme->render_start_matcher_length - 1)
					r_scheme->render_match_buffer_index = 0;
			} else {
				has_index_change = r_scheme->render_start_matcher_index > 0;
				r_scheme->render_start_matcher_index = 0;
			}
		}
		if (r_scheme->render_start_matcher_index == r_scheme->render_end_matcher_length &&
			r_scheme->render_end_matcher_index < r_scheme->render_end_matcher_length) {
			if (full_line[read_full_line] == r_scheme->render_end_matcher[r_scheme->render_end_matcher_index]) {
				has_found_start_match = 1;
				r_scheme->render_end_matcher_index++;
			} else {
				has_index_change = r_scheme->render_end_matcher_index > 0;
				r_scheme->render_end_matcher_index = 0;
			}
		}

		// check if a character should be briefly stored in buffer to see
		// if the character is part of the start or end matcher
		if (has_found_start_match && (r_scheme->render_start_matcher_index < r_scheme->render_start_matcher_length ||
			r_scheme->render_end_matcher_index < r_scheme->render_end_matcher_length)) {

			// copy character into buffer
			r_scheme->render_match_buffer[r_scheme->render_match_buffer_index] = full_line[read_full_line];
			r_scheme->render_match_buffer_index++;

			r_scheme->render_match_buffer = resize_array(r_scheme->render_match_buffer, r_scheme->max_render_match_buffer, r_scheme->render_match_buffer_index, sizeof(char));
			r_scheme->render_match_buffer[r_scheme->render_match_buffer_index] = '\0';
		
			continue;
		}

		if (has_index_change) {
			// read render_match_buffer into full_data
			*full_data = resize_array(*full_data, full_data_max, full_data_index + r_scheme->render_match_buffer_index, sizeof(char));

			strcat(*full_data, r_scheme->render_match_buffer);
			r_scheme->render_match_buffer_index = 0;
		}

		// have render_start_matcher, start reading directly into render_match_check
		if (r_scheme->render_start_matcher_index == r_scheme->render_start_matcher_length &&
			r_scheme->render_end_matcher_index != r_scheme->render_end_matcher_length) {

			r_scheme->render_match_check[r_scheme->render_match_check_index] = full_line[read_full_line];
			r_scheme->render_match_check_index++;

			r_scheme->render_match_check = resize_array(r_scheme->render_match_check, r_scheme->max_render_match_check, r_scheme->render_match_check_index, sizeof(char));
			r_scheme->render_match_check[r_scheme->render_match_check_index] = '\0';

			continue;
		}
		// find render_end_matcher:
			// close connection
		if (r_scheme->render_start_matcher_index == r_scheme->render_start_matcher_length &&
			r_scheme->render_end_matcher_index == r_scheme->render_end_matcher_length) {
			// see what should be in the place of this found word:
			char *replacer = get__hashmap(r_scheme->res_render_matches, r_scheme->render_match_check, "");
			
			if (!replacer) {
				scheme_reset(r_scheme);
				continue;
			}

			int replacer_len = strlen(replacer);

			*full_data = resize_array(*full_data, full_data_max, full_data_index + replacer_len, sizeof(char));

			strcat(*full_data, replacer);

			// reset data
			scheme_reset(r_scheme);

			full_data_index += replacer_len;
		} else {
			// add to full_data
			(*full_data)[full_data_index] = full_line[read_full_line];
			full_data_index++;

			(*full_data)[full_data_index] = '\0';
		}
	}

	return full_data_index;
}
/* RESULT SENDERS */
int res_sendFile(res_t res, char *name) {
	res_t *res_pt = res.res_self;

	if (res_pt->render && (!res_pt->matchStart || !res_pt->matchEnd)) {
		printf("\033[0;31m");
		printf("\n** Render match schema failed -- missing: %s%s%s **\n",
			res_pt->matchStart ? "" : "start match ", !res_pt->matchStart && !res_pt->matchEnd ?
			"and " : "", res_pt->matchEnd ? "" : "end match");
		printf("\033[0;37m");

		char *err_msg = malloc(sizeof(char) * (27));
		sprintf(err_msg, "Render match schema failed");
		data_send(res.socket, res.status_code, 500, "-t", err_msg);
		free(err_msg);

		return 0;
	}

	// load full file path
	int full_fpath_len = strlen(res.__dirname ? res.__dirname : getenv("PWD")) + strlen(name) +
		(name[0] == '/' ? -1 : 0) + 1 + (!res.__dirname ? 1 : 0);
	char *full_fpath = malloc(sizeof(char) * full_fpath_len);
	strcpy(full_fpath, res.__dirname ? res.__dirname : getenv("PWD"));
	if (!res.__dirname)
		strcat(full_fpath, "/");
	strcat(full_fpath, name + (name[0] == '/' ? 1 : 0));

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

	// if file opens, read from file to create a new request
	size_t read_line_size = sizeof(char) * 64;
	char *read_line = malloc(read_line_size);

	int *full_data_max = malloc(sizeof(int)), full_data_index = 0;
	*full_data_max = 64;
	char *full_data = malloc(sizeof(char) * *full_data_max);
	full_data[0] = '\0';

	/* RENDER SCHEMES */
	render_scheme_t *r_scheme = res_pt->render ? create_render_scheme(res_pt) : NULL;

	int curr_line_len = 0;
	while ((curr_line_len = getline(&read_line, &read_line_size, f_pt)) != -1) {
		full_data = resize_array(full_data, full_data_max, full_data_index + curr_line_len + 1, sizeof(char));

		// check for if any rendering calculations should occur
		if (res_pt->render) {
			full_data_index = check_renders(r_scheme, read_line, &full_data, full_data_max, full_data_index);
		} else {
			strcat(full_data, read_line);
			full_data_index += curr_line_len;
		}

		full_data[full_data_index] = '\0';
	}


	// if res_matches() is used, but not rendering, free
	if (!r_scheme && res_pt->render_matches)
		deepdestroy__hashmap(res_pt->render_matches);

	if (r_scheme)
		destroy_render_scheme(r_scheme);

	free(read_line);
	fclose(f_pt);

	data_send(res.socket, res.status_code, 200, "-i", full_fpath, full_data, full_data_index);
	
	free(full_fpath);
	free(full_data_max);
	free(full_data);
	return 0;
}

int res_end(res_t res, char *data) {
	data_send(res.socket, res.status_code, 200, "-t", data);

	return 0;
}

int res_matches(res_t res, char *match, char *replacer) {
	res_t *res_pt = res.res_self;

	if (!res_pt->render_matches)
		res_pt->render_matches = make__hashmap(0, NULL, NULL);

	insert__hashmap(res_pt->render_matches, match, replacer, "", compareCharKey, NULL);

	return 0;
}

int res_render(res_t res, char *name, char *match_start, char *match_end) {
	res_t *res_pt = res.res_self;
	res_pt->render = 1;

	// open file:
	char *full_file_name = malloc(sizeof(char) * (strlen(name) + ((res_pt->fileName && res_pt->fileName[0] == 'f') ? 0 : 6)));
	strcpy(full_file_name, name);
	printf("%d %s\n", strlen(name) + ((res_pt->fileName && res_pt->fileName[0] == 'f') ? 0 : 5), full_file_name);
	if (!res_pt->fileName)
		strcat(full_file_name, ".html");

	res_pt->matchStart = match_start;
	res_pt->matchEnd = match_end;
	
	int response = res_sendFile(res, full_file_name);

	free(full_file_name);
	deepdestroy__hashmap(res_pt->render_matches);

	return response;
}

int fsck_directory(char *major_path, char *minor_fp) {
	// combine the char *:
	char *full_path = malloc(sizeof(char) * (strlen(major_path) + strlen(minor_fp)));

	strcpy(full_path, major_path);
	strcat(full_path, minor_fp + sizeof(char)); // remove first "/"

	FILE *f_ck = fopen(full_path, "r");

	int have_file = f_ck ? 1 : 0;

	if (f_ck)
		fclose(f_ck);

	free(full_path);

	return have_file;
}