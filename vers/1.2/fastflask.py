import flask
from gevent.pywsgi import WSGIServer
from flask import request, render_template
from subprocess import Popen

import json
import uuid
import time

import win32pipe
import win32file

app = None
TIMEOUT = 2 # timeout in seconds
CHUNKSIZE = 100000 # chunk size when reading from named pipe

def getAPIResponse(url_path, method, json_data, json_headers):

    # write data to named pipe
    rid = str(uuid.uuid4())
    pipe_name = fr'\\.\pipe\.requests\{rid}'
    pipe = win32pipe.CreateNamedPipe(
        pipe_name,
        win32pipe.PIPE_ACCESS_DUPLEX,
        win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE | win32pipe.PIPE_WAIT,
        win32pipe.PIPE_UNLIMITED_INSTANCES,
        65536, 65536, 0, None
    )
    win32pipe.ConnectNamedPipe(pipe, None)
    win32file.WriteFile(pipe, f"{url_path}\n{method}\n{json_data}\n{json_headers}".encode())

    # wait for data in the pipe and timeout
    start = time.time()
    while True:
        d, _, __ = win32pipe.PeekNamedPipe(pipe, 1)
        if d:
            break
        if time.time() - start >= TIMEOUT:
            win32pipe.DisconnectNamedPipe(pipe)
            win32file.CloseHandle(pipe)
            return "resp.status = 504", f"<h1>Timeout</h1><p>Request timed out after {TIMEOUT} seconds.</p><br><i>this speedy server is powered by fastflask.</i>"

    # get response from named pipe and close it
    win32file.SetFilePointer(pipe, 0, win32file.FILE_BEGIN)
    _, data = win32file.ReadFile(pipe, CHUNKSIZE, None)
    res = data
    while len(data) == CHUNKSIZE:            
        _, data = win32file.ReadFile(pipe, CHUNKSIZE, None)
        res += data
    res = res.decode().split("__TO_EXEC||TO_RETURN||DELIM__")
    win32pipe.DisconnectNamedPipe(pipe)
    win32file.CloseHandle(pipe)
    return res

def link(_app):
    global app
    app = _app

def start(exe_path, host="127.0.0.1", port=5000):

    # open backend process
    proc = Popen(exe_path)

    # pass all requests to c++ backend if 404
    @app.errorhandler(404)
    def cpp_backend(e):
        json_data = "{}"
        if request.content_type == "application/json":
            if request.method == "GET" and request.args:
                json_data = json.dumps(request.args.to_dict())
            else:
                json_data = json.dumps(request.json)
        to_exec, to_return = getAPIResponse(
            request.path + ("/" if request.path[-1] != "/" else ""),
            request.method, json_data, json.dumps(dict(request.headers)),
        )
        resp = flask.make_response() if not to_return.startswith("render_template") else flask.make_response(eval(to_return)) # hacks
        exec(to_exec)
        if not to_return.startswith("render_template"): resp.set_data(to_return)
        return resp

    http_server = WSGIServer((host, port), app)
    http_server.serve_forever()