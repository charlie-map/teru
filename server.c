#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "express.h"
#include "request.h"

#define HOST "localhost"
#define PORT "8888"
#define PATH_MAX 200

void home_page(req_t req, res_t res) {
	printf("received request %s %s\n", req.url, req.type);

	char *name = req_query(req, "name");
	printf("name is %s\n", name);

	printf("%d %s\n", res.socket, res.__dirname);
	res_sendFile(res, "home");

	printf("end\n");

	return;
}

int main() {
	app new_server = express();

	char *setup_public_dir = malloc(sizeof(char) * PATH_MAX);
	if (!getcwd(setup_public_dir, sizeof(char) * PATH_MAX)) {
		fprintf(stderr, "getcwd() error\n");
		exit(1);
	}

	strcat(setup_public_dir, "/public/");
	app_use(new_server, "/", setup_public_dir);

	char *setup_views_dir = malloc(sizeof(char) * PATH_MAX);
	if (!getcwd(setup_views_dir, sizeof(char) * PATH_MAX)) {
		fprintf(stderr, "getcwd() error\n");
		exit(1);
	}

	strcat(setup_views_dir, "/views/");
	app_set(new_server, "views", setup_views_dir);

	// setup listener routes
	app_get(new_server, "/", home_page);

	int status = app_listen(HOST, PORT, new_server);

	return 0;
}