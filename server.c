#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <pthread.h>

#include "express.h"
#include "request.h"

#define PORT "8888"

int home_page(req_t *req, res_t *res) {
	
}

int main() {
	int status = listen(PORT);

	return 0;
}