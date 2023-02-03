# FastFlask

A python module for windows to link a C++ backend to flask.

Visit the [FastFlask Website](https://fastflask.plex7090.repl.co)

## Dependencies

Dependencies: flask, gevent, pywin32

Use `pip install dependency_name` to install dependencies.

## Usage

Compile `server.cpp`, then run `server.py`.

In server.py:

```py
from flask import Flask

import fastflask

app = Flask(__name__) # create flask app

# create a python route
@app.route("/example_py/", methods=["GET"])
def example_py():
    return "example response from python"

fastflask.link(app) # link app to fastflask
fastflask.start("server.exe") # start the server and link to c++ executable. kwargs: (host: str, port: int), defaults to 127.0.0.1 and 5000
```

In server.cpp:

```cpp
#include <iostream>

#include "fastflask.hpp"

int main(){

    // create a c++ route.
    ff::add_route("/example_cpp/", ff::GET, [](json j, json headers, json dynamic_vals){
        return ff::RES("", "example response from c++");
    });
    
    // a dynamic route. it works like how it would in vanilla flask.
    // 'dynamic_vals' contain the values substituted into the dynamic url.
    ff::add_route("/users/<username>/", ff::GET, [](json j, json headers, json dynamic_vals){
        return ff::RES("", "hello, " + std::string(dynamic_vals["username"]) + "!");
    });
    
    // you can also use flask's render_template function from the c++ backend and this function demonstrates how.
    // to modify the python RESPONSE object, use the name 'resp' in the 'to_exec' string.
    ff::add_route("/", ff::GET, [](json j, json headers, json dynamic_vals){
        return ff::RES("resp.set_cookie('visitedbefore', 'true')", "", "index.html");
    });
    
    // ff::RES takes 5 arguments: to_exec, to_return, r_template="", kwargs={}, code=200
    ff::add_route("/test_route/", ff::GET, [](json j, json headers, json dynamic_vals){
        return ff::RES(
            "resp.set_cookie('cookies', 'true')", // to_exec can contain python code to be executed from the python side, such as setting cookies.
            "", // to_return can contain data to be returned when there is no template to render.
            "test_route.html", // r_template is optional. it can contain a template file path.
            {{"username": "john"}}, // kwargs is optional. it can be used to pass context variables to the rendered template.
            200 // code is optional. it is the status code returned by the server.
        );
    }
    
    // start the server. running this code by itself without calling from python does nothing.
    ff::start();
}
```
