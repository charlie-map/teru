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
	res_sendFile(res, "home.html");

	printf("end\n");

	return;
}

int main() {
	app new_server = express();

	app_use(new_server, "/", getenv("PWD"), "/public/");
	app_set(new_server, "views", getenv("PWD"), "/views/");

	// setup listener routes
	app_get(new_server, "/", home_page);

	int status = app_listen(HOST, PORT, new_server);

	return 0;
}