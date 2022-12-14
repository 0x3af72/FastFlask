import flask
from flask import request, render_template
from subprocess import Popen, PIPE
from werkzeug.routing import RequestRedirect, MethodNotAllowed, NotFound

import os
import json
import uuid

app = None

def getAPIResponse(url_path, method, json_data):

    rid = str(uuid.uuid4())

    # write json first because fastflask.hpp only checks for .rq file so json might not be there when read
    with open(f".requests/{rid}-json.rq", "w") as rq_w:
        rq_w.write(f"{json_data}\n")
    with open(f".requests/{rid}.rq", "w") as rq_w:
        rq_w.write(f"{url_path}\n{method}\n")

    # waiting for response
    while True:
        req_folder = os.listdir(".requests")
        if (f"{rid}-exec.rq" in req_folder) and (f"{rid}-return.rq" in req_folder):

            # check if result is still being written
            done_writing = False
            try:
                os.rename(f".requests/{rid}-exec.rq", f".requests/{rid}-exec.rq")
                os.rename(f".requests/{rid}-return.rq", f".requests/{rid}-return.rq")
                done_writing = True
            except OSError:
                pass

            if done_writing:
                with open(f".requests/{rid}-exec.rq", "r") as rq_r:
                    to_exec = rq_r.read()
                with open(f".requests/{rid}-return.rq", "r") as rq_r:
                    to_return = rq_r.read()
                os.remove(f".requests/{rid}-exec.rq")
                os.remove(f".requests/{rid}-return.rq")
                return to_exec, to_return

def link(_app):
    global app
    app = _app

def start(exe_path):

    # open backend process and create directory for handling requests
    proc = Popen(exe_path, stdin=PIPE, stdout=PIPE, stderr=PIPE, encoding="UTF8")
    if not ".requests" in os.listdir("."):
        os.mkdir(".requests")
    else:
        for file in os.listdir(".requests"):
            os.remove(f".requests/{file}")

    # pass all requests to c++ backend if 404
    @app.errorhandler(404)
    def cpp_backend(e):
        resp = flask.make_response()
        json_data = None
        if request.method == "GET" and request.args:
            json_data = json.dumps(request.args.to_dict())
        else:
            json_data = json.dumps(request.json)
        to_exec, to_return = getAPIResponse(request.path, request.method, json_data)
        exec(to_exec)
        if to_return.startswith("render_template"):
            return eval(to_return)
        resp.set_data(to_return)
        return resp
    
    app.run()