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
#include "request.h"
#include "hashmap.h"

#define MAXLINE 4096

typedef struct Listener {
	char *r_type; // "GET" / "POST" / etc.

	void (*handler)(req_t *, res_t *); // handles processing for the request
									   // made by user

	/* more to come! */
} listen_t;

listen_t *new_listener(char *r_type, void (*handler)(req_t *, res_t *)) {
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
/*
	Express Struct:
		This will handle pretty much everything
		Any functions basically just alter whatever is in here
*/
struct Express {
	struct Express *my_ptr; // points to memory for this express app

	// routes for different request types (currently on GET and POST)
	hashmap *routes;

	/* currently only hashes string to string */
	hashmap *app_settings;
	/*
		`use`d parameters -- with concatenated string ("use")
		`set` parameters -- with concatenated string ("set")

		the concatenation is how to differentiate between different
		settings while using a single hashmap object
	*/

	socket_t *socket;

	int server_open; // for evaluting if the server is trying to close
	/* more to come! */
};

/* setup an initial service */
app express() {
	app *app_t = malloc(sizeof(app));

	app_t->routes = make__hashmap(1, print_listen_t, free_listen_t);
	app_t->app_settings = make__hashmap(0, print_app_settings, NULL);

	app_t->server_open = 1;

	return *app_t;
}

void acceptor_function(void *app_ptr);

int listen(char *PORT, app app_t) {
	socket_t *socket = get_socket(NULL, PORT);

	if (listen(socket->sock, BACKLOG) == -1) {
		perror("listen error");
		return 1;
	}

	app_t.socket = socket;

	// start acceptor thread
	pthread_t accept_thread;
	int check = pthread_create(&accept_thread, NULL, &acceptor_function, app_t->my_ptr);



	printf("server go vroom\n");

	destroy_socket(socket);

	return 0;
}

/* ROUTE BUILDER -- POINTS TO GENERIC ROUTE BUILDER
	|__ the only reason for these functions is to build a slightly
		simpler interface on top of the library */
int build_new_route(app app_t, char *type, char *endpoint, void (*handler)(req_t *, res_t *)) {
	// check that the route doesn't exist (assumes the type matches)
	hashmap__response *routes = (hashmap__response *) get__hashmap(app_t.routes, endpoint);

	for (int check_route = 0; check_route < routes->payload__length; check_route) {
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

int get(app app_t, char *endpoint, void (*handler)(req_t *, res_t *)) {
	return build_new_route(app_t, "get", endpoint, handler);
}

int post(app app_t, char *endpoint, void (*handler)(req_t *, res_t *)) {
	return build_new_route(app_t, "post", endpoint, handler);
}

/* EXPRESS APP SETTINGS BUILDER */
void use(app app_t, char *route, char *descript) {
	// update route name to have a "use" at the beginning

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 'u'; new_route_name[1] = 's'; new_route_name[2] = 'e';
	strcat(new_route_name, route);

	insert__hashmap(app_t->app_settings, new_route_name, descript);

	return;
}

void set(app app_t, char *route, char *descript) {
	// update route name to have a "set" at the beginning

	char *new_route_name = malloc(sizeof(char) * (strlen(route) + 4));
	new_route_name[0] = 's'; new_route_name[1] = 'e'; new_route_name[2] = 't';
	strcat(new_route_name, route);

	insert__hashmap(app_t->app_settings, new_route_name, descript);

	return;
}


typedef struct ConnectionHandle {
	int p_handle; // socket specific pointer

	app *app_t; // all the meta data
} ch_t;
/* LISTENER FUNCTIONS FOR SOCKET */
// Based on my trie-suggestor-app (https://github.com/charlie-map/trie-suggestorC/blob/main/server.c) and
// Beej Hall websockets (http://beej.us/guide/bgnet/html/)
void connection(void *app_ptr) {
	int new_fd = ((ch_t *) app_ptr)->p_handle;
	app app_t = *((ch_t *) app_ptr)->app_t;

	int recv_res = 1;
	char *buffer = malloc(sizeof(char) * MAXLINE);
	int buffer_len = MAXLINE;

	// make a continuous loop for new_fd while they are still alive
	// as well as checking that the server is running
	while (recv_res = recv(new_fd, buffer, buffer_len, 0) && app_t.server_open) {
		if (recv_res == -1) {
			perror("receive: ");
			continue;
		}

		if (buffer_len == 0) // still waiting
			continue;

		// read data...
		printf("%s\n", buffer);
	}

	// if the close occurs due to thread_status, send an error page
	if (!*client_data->thread_status);

	close(new_fd);
	free(buffer);
	free(client_data);

	pthread_t *retval;
	// close thread
	pthread_exit(retval);
}

void acceptor_function(void *app_ptr) {
	app app_t = *(app *) app_ptr;

	int sock_fd = ;
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
		pthread_t socket;
		pthread_create(&socket, NULL, &connection, app_ptr);
	}

	return 0;
}