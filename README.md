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

fastflask.link(app) # link app to fastfalsk
fastflask.start("server.exe") # start the server and link to c++ executable
```

In server.cpp:

```cpp
#include <iostream>

#include "fastflask.hpp"

int main(){

    // create a c++ route
    ff::add_route("/example_cpp/", ff::GET, [](json j){
        return ff::RES("", "example response from c++");
    });
    
    // start the server. running this code by itself without calling from python does nothing.
    ff::start();
}
```
