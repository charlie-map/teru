#include <stdio.h>
#include <stdlib.h>

#include "teru.h"

#define HOST "localhost"
#define PORT "8888"

void home_page(req_t req, res_t res) {
	printf("received request %s %s\n", req.url, req.type);

	char *name = req_query(req, "name");
	printf("name is %s\n", name);

	printf("%d %s\n", res.socket, res.__dirname);
	res_sendFile(res, "home.html");

	return;
}

void different_page(req_t req, res_t res) {
	res_end(res, "Test send");

	return;
}

int main() {
	teru_t app = teru();

	app_use(app, "/", getenv("PWD"), "/public/");
	app_set(app, "views", getenv("PWD"), "/views/");

	// setup listener routes
	app_get(app, "/", home_page);
	app_get(app, "/test", different_page);

	int status = app_listen(HOST, PORT, app);

	return 0;
}