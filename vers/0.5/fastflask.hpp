#include <iostream>
#include <windows.h>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <functional>
#include <unordered_map>

#include "nlohmann_json.hpp"

#pragma once

using json = nlohmann::json;

// suffix function
bool string_ends_with(std::string f, std::string s){
    if (s.size() > f.size()) return false;
    return (f.substr(f.size() - s.size(), s.size()) == s);
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
        std::vector<ROUTE_NODE> children; // other nodes
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
            // search for new nodes
            bool unmatched = true;
            for (ROUTE_NODE& node: cur_node->children){
                if (node.wildcard && is_wildcard){
                    unmatched = false;
                    cur_node = &node;
                    break;
                } else if (node.match == match){
                    unmatched = false;
                    cur_node = &node;
                    break;
                }
            }
            if (unmatched) creating = true;
        }

        if (creating){
            // no more matches, create new nodes
            ROUTE_NODE new_node(match, is_wildcard, dv_name);
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
    for (ROUTE_NODE node: cur_node->children){
        if (node.match == check_match){
            find_route(&node, matches, type, dv_temp, route_exists, matched_func, dynamic_vals);
        }
        if (node.wildcard){
            json new_dv_temp = dv_temp;
            new_dv_temp[node.dv_name] = check_match;
            find_route(&node, matches, type, new_dv_temp, route_exists, matched_func, dynamic_vals);
        }
    }
}

void get_response(
    std::string rid, std::string url_path, std::string method,
    std::string json_data, std::string json_headers
){

    // logging purposes
    std::cout << "[FASTFLASK] " << method << " " << url_path << " request received.\n";

    // match url path
    bool route_exists = false;
    std::function<RES(json, json, json)> matched_func;
    json dynamic_vals = {};
    find_route(&routes, split_string(url_path, "/"), m_to_rt[method], {}, route_exists, matched_func, dynamic_vals);

    // return a response
    std::string to_exec, to_return;
    if (route_exists){
        RES r = matched_func(json::parse(json_data), json::parse(json_headers), dynamic_vals);
        to_exec = "resp.status = " + std::to_string(r.code) + "\n";
        to_exec += r.to_exec;
        to_return = r.to_return;
    } else {
        // error
        to_exec = "resp.status = 404";
        to_return = "<h1>404 Not Found</h1><h4>Invalid url route, or request method.</h4>" + url_path + "<br><i>this speedy server is powered by fastflask.</i>";
    }

    // write to exec and return files and exit function
    std::ofstream rq_w; rq_w.open(".requests/" + rid + "-exec.rq");
    rq_w << to_exec; rq_w.close();
    std::ofstream rq_w_return; rq_w_return.open(".requests/" + rid + "-return.rq");
    rq_w_return << to_return; rq_w_return.close();
    std::cout << "[FASTFLASK] " << method << " " << url_path << " request completed.\n";
}

void start(float sleep_dur = 0){

    // debug message
    std::cout << "[FASTFLASK] Server started.\n";

    // data that we will get from fastflask.py
    std::string url_path, method, json_data, json_headers;

    while (true){

        // find all unanswered requests
        for (auto file: std::filesystem::directory_iterator(".requests")){

            // grab filepath
            std::string file_path = file.path().string();

            // ignore return files and json
            if (!string_ends_with(file_path, "-complete.rq")){
                continue;
            }
            file_path = file_path.substr(0, file_path.size() - std::string("-complete.rq").size()) + ".rq";

            // get request data
            std::string rid = file_path.substr(std::string(".requests/").size(), file_path.size() - std::string(".rq").size() - std::string(".requests/").size());
            std::ifstream rq_r; rq_r.open(file_path);
            std::getline(rq_r, url_path); std::getline(rq_r, method);
            std::getline(rq_r, json_data); std::getline(rq_r, json_headers);
            rq_r.close();

            // delete files
            remove(file_path.c_str());
            remove((".requests/" + rid + "-complete.rq").c_str());

            // new thread to return response
            std::thread resp_thread(get_response, rid, url_path, method, json_data, json_headers);
            resp_thread.detach();
        }

        Sleep(sleep_dur); // incase user for some reason doesnt want high cpu usage
    }
}

}