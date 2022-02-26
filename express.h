#ifndef __EXPRESS_L__
#define __EXPRESS_L__

#define BACKLOG 10

#include "request.h"
#include "hashmap.h"

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


// basic req, res structures
typedef struct ReqStruct {
	char *url; // full url of request
	char **query, **body; // any parameters involved
} req_t;
typedef struct ResStruct {
	char *end; // NOT DONE
} res_t;

// used for creating a new listener to attach to the app
typedef struct Express app;

app express();
void app_use(app app_t, char *route, char *descript);
void app_set(app app_t, char *route, char *descript);

int app_get(app app_t, char *endpoint, void (*handler)(req_t, res_t));
int app_post(app app_t, char *endpoint, void (*handler)(req_t, res_t));
/* more route types to come if necessary */

int app_listen(char *PORT, app app_t);

#endif