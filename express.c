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

int listen(char *PORT) {
	socket_t *app = get_socket(NULL, PORT);

	if (listen(app->sock, BACKLOG) == -1) {
		perror("listen error");
		return 1;
	}

	printf("server go vroom\n");

	destroy_socket(app);

	return 0;
}