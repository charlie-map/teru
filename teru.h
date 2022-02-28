#ifndef __TERU_L__
#define __TERU_L__

#define BACKLOG 10

#include "request.h"
#include "hashmap.h"

/*
	Teru Struct:
		This will handle pretty much everything
		Any functions basically just alter whatever is in here
*/
typedef struct Teru {
	struct Teru *app_ptr;

	hashmap *status_code; // holds the code -> textual phrase pair
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

	int server_active; // for evaluting if the server is trying to close
	/* more to come! */
} teru_t;


// basic req, res structures
typedef struct HeaderMap {
	char *type; // request type
	char *url; // request url
	char *http_stat; // request status

	hashmap *meta_header_map;

	hashmap *query_map;
	hashmap *body_map;
} req_t;
typedef struct ResStruct {
	int socket;
	hashmap *status_code;

	char *__dirname;
} res_t;

teru_t teru();
void app_use(teru_t app, char *route, ...);
void app_set(teru_t app, char *route, ...);

int app_get(teru_t app, char *endpoint, void (*handler)(req_t, res_t));
int app_post(teru_t app, char *endpoint, void (*handler)(req_t, res_t));
/* more route types to come if necessary */

int res_sendFile(res_t res, char *name);
int res_end(res_t res, char *data);

char *req_query(req_t req, char *name);
char *req_body(req_t req, char *name);

int app_listen(char *HOST, char *PORT, teru_t app);

#endif