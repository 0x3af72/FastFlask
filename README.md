# FastFlask
A python module to link a C++ backend to flask.

## Usage

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
fastflask.start("server.exe") # start the server and link to c++ executable
```

In server.cpp:

```cpp
#include <iostream>

#include "fastflask.hpp"

int main(){

    // create a c++ route
    ff::add_route("/example_cpp/", ff::GET, [](json j, json dynamic_vals){
        return ff::RES("", "example response from c++");
    });
    
    // a dynamic route. it works like how it would in vanilla flask.
    // dynamic_vals contain the values substituted into the dynamic url.
    ff::add_route("/users/<username>/", ff::GET, [](json j, json dynamic_vals){
        std::string template_file = std::string(dynamic_vals["username"]) + ".html";
        return ff::RES("", "render_template('" + template_file + "')");
    });
    
    // start the server. running this code by itself without calling from python does nothing.
    ff::start();
}
```
