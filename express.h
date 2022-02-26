#ifndef __EXPRESS_L__
#define __EXPRESS_L__

#define BACKLOG 10

// basic req, res structures
typedef struct ReqStruct {
	char *url; // full url of request
	char **query, **body; // any parameters involved
} req_t;
typedef struct ResStruct {

} res_t;

// used for creating a new listener to attach to the app
typedef struct Express app;

app express();
void use(app app_t, char *route, char *descript);
void set(app app_t, char *route, char *descript);

int get(app app_t, char *endpoint, void (*handler)(req_t *, res_t *));
int post(app app_t, char *endpoint, void (*handler)(req_t *, res_t *));
/* more route types to come if necessary */

int listen(char *PORT, app app_t);

#endif