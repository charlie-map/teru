# Meet Teru!
### Charlie Hall

This repository is an implementation (rework, build, etc.) of [nodejs Express](https://expressjs.com/) packed in a library to support easier server building in C. This functionality matches very closely with many functions in Express (if not quite as built out as Express currently). This README goes through the process of creating a simple web server and how each component works.
The following code is an example of a simple **Teru** server that sends a simple HTML file `home.html` when a socket joins and goes to `/` on the page:

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

1. [teru() -- new Teru](#new-teru)
## App functionalities:
2. [app_use() -- Public directories and library usage](#use-teru)
3. [app_set() -- Views directory](#set-teru)
## Send functionalities:
4. [res_sendFile() -- Send a file](#send-file-to-user)
5. [res_end() -- Send a string](#send-message-to-user)
6. [res_render() -- Send a file with match keys that replace to allow for dynamic HTML pages](#render-file-to-user)
## Request parameters:
6. [req_query()](#see-request-query-parameters)
7. [req_body()](#see-request-body-parameters)

<img src="http://charlie.city/teru_art.png"/>

# New Teru
`teru()` returns a `teru_t` struct which will have some default parameters set but remains mostly untouched. The functions following this will get into how to personalize **Teru** for your project. From the example code above, creating a new **Teru** instance just involves running:

```C
teru_t app = teru();
```

This `app` variable will be used throughout the following function examples.

# Use Teru
The `app_use` function will be how to add any extra components onto the Teru instance.
***Not done***

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

# Render File to User
This allows for a server to take an HTML page and find and replace occurences of _match strings_. There are a few steps for setting this up:
- Create an HTML file with match strings. The _start match_ and _end match_ can be whatever strings you wish. However, these strings must match what you give the `render()` function in the following steps.

```HTML
<html>
	<head>
		<title>Render Example</title>
	</head>
	<body>
		<h1>Hi there {{NAME}}!</h1>
	</body>
</html>
```
- Next, set the match parameters using the `res_matches()` function, which for the previous example would look like the following. Note that `res` references the second parameter of the handler function (see [res_sendFile](#Send-File-to-User) for an example).
```C
res_matches(res, "NAME", "charlie-map");
```
- Finally, use the `res_render()` function to interpret the match strings and send the result to the user. `res_render()` takes in `res`, the name of the file, and the _start match_ and _end match_. Assuming the above HTML file is named "home.html", the `res_render()` call would look like:

```C
res_render(res, "home", "{{", "}}");
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

- `app_put()`, `app_delete()`, etc.
- more `app_set()` functionality
