import flask
from gevent.pywsgi import WSGIServer
from flask import request, render_template
from subprocess import Popen, PIPE

import os
import json
import uuid
import time

app = None
TIMEOUT = 2 # timeout 10 seconds

def getAPIResponse(url_path, method, json_data):

    rid = str(uuid.uuid4())

    # write to files
    with open(f".requests/{rid}.rq", "w") as rq_w:
        rq_w.write(f"{url_path}\n{method}\n{json_data}\n")
    with open(f".requests/{rid}-complete.rq", "w") as rq_w:
        pass # just a simple flag to indicate write is complete

    # waiting for response
    start_time = time.time()
    while True:

        # timeout
        if time.time() - start_time >= TIMEOUT:
            return "resp.status = 408", f"<h1>Timeout</h1><p>Request timed out after {TIMEOUT} seconds.</p>"

        # check if result has been written
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

def start(exe_path, host="127.0.0.1", port=5000):

    # open backend process and create directory for handling requests
    # proc = Popen(exe_path, stdin=PIPE, stdout=PIPE, stderr=PIPE, encoding="UTF8")
    proc = Popen(exe_path)
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
        to_exec, to_return = getAPIResponse(
            request.path + ("/" if request.path[-1] != "/" else ""),
            request.method, json_data
        )
        exec(to_exec)
        if to_return.startswith("render_template"):
            return eval(to_return)
        resp.set_data(to_return)
        return resp

    http_server = WSGIServer((host, port), app)
    http_server.serve_forever()