# Meet Teru!
### Charlie Hall

This repository is an implementation (rework, build, etc.) of [nodejs Express](https://expressjs.com/) packed in a library to support easier server building in C. This functionality matches very closely with many functions in Express (if not quite as built out as Express currently). This README goes through the process of creating a simple web server and how each component works.
The following code is an example of a simple ExpressC server that sends a simple HTML file `home.html` when a socket joins and goes to `/` on the page:

```C
#include <stdio.h>
#include <stdlib.h>

#include "teru.h"

#define HOST "localhost"
#define PORT "8888"

void home_page(req_t req, res_t res) {
	res_end("Hi there, welcome to the home page");
}

int main() {
  // initialize the server
	teru_t app = teru();

	// setup listener routes
	app_get(app, "/", home_page);

  // listen on the port and host defined above
	app_listen(HOST, PORT, app);

	return 0;
}
```

Wow! Looks pretty similar to the Express functions. Below lays out the current functions within **Teru**:

1. [teru() -- new Teru](#New-Teru)
## App functionalities:
2. [app_use()](#Use-Teru)
3. [app_set()](#Set-Teru)
## Send functionalities:
4. [res_sendFile()](#Send-File-to-User)
5. [res_end()](#Send-Message-to-User)
## Request parameters:
6. [req_query()](#See-Request-Query-Parameters)
7. [req_body()](#See-Request-Body-Parameters)

<img src="http://charlie.city/teru_art.png"/>

# New Teru
`teru()` returns a `teru_t` struct which will have some default parameters set but remains mostly untouched. The functions following this will get into how to personalize **Teru** for your project. From the example code above, creating a new **Teru** instance just involves running:

```C
teru_t app = teru();
```

This `app` variable will be used throughout the following function examples.

# Use Teru
The `app_use` function will be how to add any extra components onto the Teru instance.
***Note done***

# Set Teru
To `app_set`, this takes in a key and a value pair for what to load in. For example, loading a `views` directory would look like:

```C
app_set(app, "views", getenv("PWD"), "/views/");
```

`getenv("PWD")` is the current working directory of the server file. Then sending any files look in this directory for the file instead of the default behavior of looking in the same directory as the main program.
*Currently `views` is the only functionality for `app_set()`, more to come in the future.*

# Send File to User
`res_sendFile()` takes in the name of a file from which to read the data. This data is then sent as the body of the response to the user. If `home.html` contained:
```HTML
<!DOCTYPE html>
<html>
	<head>
		<title>Example Page</title>
	</head>
	<body>
		<h1>Hey there!!!</h1>
	</body>
</html>
```

Then attaching this to a listener would involve creating a simple listener that watches for requests at `/`:

```C
void home_page(req_t req, res_t res) {
	res_sendFile(res, "home.html");
}
```

Then attaching to `/` would involve using `app_get()` which then handles the rest:

```C
app_get(app, "/", home_page);
```

**Teru** is ready to say `Hey there!!!` to all the interested users!

# Send Message to User
`res_end()` works similarly to [`res_sendFile()`](#Send-File-to-User), but instead of a file name as the second parameter takes in a string message:

```C
void hello_there(req_t req, res_t res) {
  res_end(res, "Hello there.");
}
```
Then connect to an endpoint:
```C
app_get(app, "/hi", hello_there);
```

# See Request Query Parameters
Request query parameters are added at the end of the URL (for example `localhost:8888/hi?name=Charlie`). `req_query()` allows access to these by inserting the name of the query parameter:

```C
void hello_name(req_t req, res_t res) {
  char *name = req_query(req, "name");
  
  printf("name: %s\n", name); // expect "Charlie" as output
  
  res_end(res, "Hi!");
}
```
Then you could easily add that name into the response string. As with previous examples, connect to **Teru** using `app_get()` or `app_post()`

# See Request Body Parameters
When using `app_post()` for a specific listener, you have access to `req_body()`. Taking the query example, instead you would have:

```C
void hello_name(req_t req, res_t req) {
  char *name = req_body(req, "name");
  
  printf("name: %s\n", name);
  
  res_end(res, "Hi!");
}
```
The only difference involves connecting the listener to **Teru** with `app_post()` instead:

```C
app_post(app, "/hi", hello_name);
```

# Teru's Future
Currently there are a few bucket list items that will be check off over time. However, feel free to [leave an issue](https://github.com/charlie-map/wiki_backend/issues) with any suggested enhancements.

- `app_use()` functionality
- `app_put()`, `app_delete()`, etc.
- more `app_set()` functionality
- `res_render()` see [Mustache Express](https://www.npmjs.com/package/mustache-express)
