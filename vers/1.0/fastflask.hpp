#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <windows.h>

#include "nlohmann_json.hpp"

#pragma once

using json = nlohmann::json;

// suffix function
bool string_ends_with(std::string f, std::string s){
    if (s.size() > f.size()) return false;
    return (f.substr(f.size() - s.size(), s.size()) == s);
}

// prefix function
bool string_starts_with(std::string f, std::string s){
    if (s.size() > f.size()) return false;
    return (f.substr(0, s.size()) == s);
}

// count chars
int count_chars(std::string s, char c){
    int n = 0;
    for (char ch: s){
        if (ch == c) n += 1;
    }
    return n;
}

// split string by delimiter
std::vector<std::string> split_string(std::string s, std::string delim){
    std::vector<std::string> res;
    while (true){
        int delim_at = s.find(delim);
        std::string token = s.substr(0, delim_at);
        if (token.size()){
            res.push_back(token);
        }
        s.erase(0, delim_at + delim.size());
        if (delim_at == std::string::npos) break;
    }
    return res;
}

namespace ff{

// response object
struct RES{
    std::string to_exec, to_return;
    int code = 200;
    RES(std::string to_exec, std::string to_return, int code = 200): to_exec(to_exec), to_return(to_return), code(code){}
};

// routes data
enum REQ_TYPE{
    GET, POST
};
std::unordered_map<std::string, REQ_TYPE> m_to_rt = {{"GET", GET}, {"POST", POST}};
std::unordered_map<REQ_TYPE, std::string> rt_to_m = {{GET, "GET"}, {POST, "POST"}};

// route "trie"?
struct ROUTE_NODE{
    public:
        std::string match; // match on this level
        bool wildcard = false; // dynamic or not
        std::string dv_name; // dynamic variable name
        std::unordered_map<std::string, int> children_idx; // other nodes
        std::vector<ROUTE_NODE> children;
        std::unordered_map<
            REQ_TYPE,
            std::function<RES(json, json, json)>
        > funcs; // function to invoke and methods
        ROUTE_NODE(std::string match, bool wildcard = false, std::string dv_name = ""){
            this->match = match;
            this->wildcard = wildcard;
            this->dv_name = dv_name;
        }
        ROUTE_NODE() = default;
};

// route trie
ROUTE_NODE routes;
void add_route(std::string url_path, REQ_TYPE type, std::function<RES(json, json, json)> func){

    // starting variables
    ROUTE_NODE* cur_node = &routes;
    bool creating = false;

    for (std::string match: split_string(url_path, "/")){

        // check if is dynamic/wildcard route
        bool is_wildcard = false;
        std::string dv_name;
        if (match.find("<") != std::string::npos && match.find(">") != std::string::npos){
            is_wildcard = true;
            dv_name = match.substr(1, match.size() - 2); // remove brackets
        }

        if (!creating){
            // search for nodes to continue
            bool unmatched = true;
            if (cur_node->children_idx.find("[WILDCARD]") != cur_node->children_idx.end() && is_wildcard){
                unmatched = false;
                cur_node = &cur_node->children[cur_node->children_idx["[WILDCARD]"]];
            } else if (cur_node->children_idx.find(match) != cur_node->children_idx.end()){
                unmatched = false;
                cur_node = &cur_node->children[cur_node->children_idx[match]];
            }
            if (unmatched) creating = true;
        }

        if (creating){
            // no more matches, create new nodes
            ROUTE_NODE new_node(match, is_wildcard, dv_name);
            cur_node->children_idx[is_wildcard ? "[WILDCARD]" : match] = cur_node->children.size();
            cur_node->children.push_back(new_node);
            cur_node = &cur_node->children[cur_node->children.size() - 1];
        }
    }

    // finally, add function to node
    cur_node->funcs[type] = func;
}

// recursive function to find route function
void find_route(
    ROUTE_NODE* cur_node,
    std::vector<std::string> matches, REQ_TYPE type, json dv_temp,
    bool& route_exists, std::function<RES(json, json, json)>& matched_func, json& dynamic_vals
){

    // check if done checking
    if (!matches.size()){
        if (cur_node->funcs.find(type) != cur_node->funcs.end()){
            route_exists = true;
            matched_func = cur_node->funcs[type];
            dynamic_vals = dv_temp;
        }
        return;
    }

    // get match currently checking
    std::string check_match = matches[0];
    matches.erase(matches.begin());

    // check any route that matches
    if (cur_node->children_idx.find("[WILDCARD]") != cur_node->children_idx.end()){
        ROUTE_NODE node = cur_node->children[cur_node->children_idx["[WILDCARD]"]];
        json new_dv_temp = dv_temp;
        new_dv_temp[node.dv_name] = check_match;
        find_route(&node, matches, type, new_dv_temp, route_exists, matched_func, dynamic_vals);
    }
    if (cur_node->children_idx.find(check_match) != cur_node->children_idx.end()){
        ROUTE_NODE node = cur_node->children[cur_node->children_idx[check_match]];
        find_route(&node, matches, type, dv_temp, route_exists, matched_func, dynamic_vals);
    }
}

void get_response(
    std::unordered_set<std::string>& handling_pipes,
    std::wstring w_pipe_name,
    std::string pipe_name
){

    // data that we will get from fastflask.py
    std::string url_path, method, json_data, json_headers;
    std::cout << pipe_name << "\n";

    // create handle
    HANDLE pipe = CreateFile(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    // read data
    char* buf = new char[1000000];
    DWORD read;
    ReadFile(pipe, buf, 999999 * sizeof(char), &read, NULL);
    buf[read] = '\0';
    std::string s_data(buf);
    delete[] buf;
    std::vector<std::string> data = split_string(s_data, "\n");
    url_path = data[0]; method = data[1]; json_data = data[2]; json_headers = data[3];

    // match url path
    bool route_exists = false;
    std::function<RES(json, json, json)> matched_func;
    json dynamic_vals = {};
    find_route(&routes, split_string(url_path, "/"), m_to_rt[method], {}, route_exists, matched_func, dynamic_vals);

    // return a response
    std::string to_exec, to_return;
    if (route_exists){
        RES r = matched_func(json::parse(json_data), json::parse(json_headers), dynamic_vals);
        to_exec = "resp.status = " + std::to_string(r.code);
        to_exec += r.to_exec;
        to_return = r.to_return;
    } else {
        // error
        to_exec = "resp.status = 404";
        to_return = "<h1>404 Not Found</h1><h4>Invalid url route, or request method.</h4>" + url_path + "<br><i>this speedy server is powered by fastflask.</i>";
    }

    // return response
    std::string resp = to_exec + "__TO_EXEC||TO_RETURN||DELIM__" + to_return;
    DWORD written;
    BOOL success = WriteFile(pipe, resp.c_str(), resp.size(), &written, NULL);
    CloseHandle(pipe);

    // remove from handling pipes
    handling_pipes.erase(handling_pipes.find(pipe_name));
}

void start(){

    // debug message
    std::cout << "[FASTFLASK] Server started.\n";
    
    // currently being handled
    std::unordered_set<std::string> handling_pipes;

    while (true){
        
        // search and handle requests
        std::wstring prefix(L"\\\\.\\pipe\\*");
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(prefix.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE){
            continue; // nothing found
        } else {
            do {
                // get pipe name in std string
                std::wstring w_pipe_name = prefix.substr(0, prefix.size() - 1) + fd.cFileName;
                std::string pipe_name(w_pipe_name.begin(), w_pipe_name.end());

                // verify pipe name is correct (under .requests)
                if (!string_starts_with(pipe_name, "\\\\.\\pipe\\.requests") || (handling_pipes.find(pipe_name) != handling_pipes.end())){
                    continue;
                }
                handling_pipes.insert(pipe_name);

                // spawn thread to return resposne
                std::thread resp_thread(get_response, std::ref(handling_pipes), w_pipe_name, pipe_name);
                resp_thread.detach();

            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }
}

}