#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <functional>
#include <filesystem>
#include <unordered_map>

#include "nlohmann_json.hpp"

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

// get regex matches
std::vector<std::string> reg_matches(std::string s, std::string r){
    std::vector<std::string> res;
    std::regex reg(r);
    auto mbegin = std::sregex_iterator(s.begin(), s.end(), reg);
    auto mend = std::sregex_iterator();
    for (auto it = mbegin; it != mend; it++){
        res.push_back(it->str());
    }
    return res;
}

namespace ff{

// response object
struct RES{
    std::string to_exec, to_return;
    int code = 200;
    RES(std::string to_exec, std::string to_return): to_exec(to_exec), to_return(to_return){}
};

// routes data
enum REQ_TYPE{
    GET, POST
};
std::unordered_map<std::string, REQ_TYPE> m_to_rt = {{"GET", GET}, {"POST", POST}};
std::unordered_map<std::string, std::unordered_map<REQ_TYPE, std::function<RES(json, json)>>> routes;
void add_route(std::string url_path, REQ_TYPE type, std::function<RES(json, json)> func){
    routes[url_path][type] = func;
    if (url_path[url_path.size() - 1] != '/'){
        routes[url_path + "/"][type] = func;
    }
}

void get_response(std::string rid, std::string url_path, std::string method, std::string json_data){

    // match url path
    bool route_exists = false;
    std::string matched_route;
    json dynamic_vals = {};
    for (auto [route, _]: routes){

        // replace placeholder values with regex strings
        std::vector<std::string> dv_names;
        std::string route_reg = route;
        for (std::string match: reg_matches(route_reg, "<(.*?)>")){
            dv_names.push_back(match.substr(1, match.size() - 2)); // remove <>
            route_reg.replace(route_reg.find(match), match.size(), "(.*?)");
        }

        // check if match
        for (std::string match: reg_matches(url_path, route_reg)){\
            if (match == url_path && count_chars(url_path, '/') == count_chars(route, '/')){

                // set variables for next part
                route_exists = true;
                matched_route = route;

                // find the values
                bool vals_done = false;
                for (int i = 0; i == i; i++){
                    int idx = route_reg.find("(.*?)");
                    if (idx == -1) break;
                    std::string val; int org_idx = idx;
                    while (url_path[idx] != '/'){
                        val += url_path[idx];
                        idx++;
                    }
                    route_reg.replace(org_idx, std::string("(.*?)").size(), val); // validify next indexes
                    dynamic_vals[dv_names[i]] = val; // add to dynamic vals dictionary
                }

                break;
            }   
        }
    }

    // return a response
    std::string to_exec, to_return;
    if (route_exists){
        if (routes[matched_route].find(m_to_rt[method]) != routes[matched_route].end()){
            // get function response
            json j = json::parse(json_data);
            RES r = routes[matched_route][m_to_rt[method]](j, dynamic_vals);
            to_exec = r.to_exec; to_return = r.to_return;
        }
    } else {
        // error
        to_exec = "resp.status = 404";
        to_return = "<h1>404 Not Found</h1><h4>(Fastflask): Invalid url route, or request method.</h4>" + url_path;
    }

    // write to exec and return files and exit function
    std::ofstream rq_w; rq_w.open(".requests/" + rid + "-exec.rq");
    rq_w << to_exec; rq_w.close();
    std::ofstream rq_w_return; rq_w_return.open(".requests/" + rid + "-return.rq");
    rq_w_return << to_return; rq_w_return.close();
}

void start(float sleep_dur = 0){

    // debug message
    std::cout << "[FASTFLASK] Server started.\n";

    // data that we will get from fastflask.py
    std::string url_path, method, json_data;

    while (true){

        // find all unanswered requests
        for (auto file: std::filesystem::directory_iterator(".requests")){

            // grab filepath
            std::string file_path = file.path().string();

            // ignore return files and json
            if (string_ends_with(file_path, "-exec.rq") || string_ends_with(file_path, "-return.rq") || string_ends_with(file_path, "-json.rq")){
                continue;
            }

            // get request data
            // note: no need to wait for file to finish writing for some reason
            std::string rid = file_path.substr(std::string(".requests/").size(), file_path.size() - std::string(".rq").size() - std::string(".requests/").size());
            std::ifstream rq_r; rq_r.open(file_path);
            std::getline(rq_r, url_path); std::getline(rq_r, method);
            rq_r.close();

            // json
            std::ifstream rq_r_json; rq_r_json.open(".requests/" + rid + "-json.rq");
            std::string tmp;
            json_data = "";
            while (std::getline(rq_r_json, tmp)){
                json_data += tmp;
            }
            rq_r_json.close();

            // delete files
            std::filesystem::remove(file_path);
            std::filesystem::remove(".requests/" + rid + "-json.rq");

            // new thread to return response
            std::thread resp_thread(get_response, rid, url_path, method, json_data);
            resp_thread.detach();
        }

        Sleep(sleep_dur); // incase user for some reason doesnt want high cpu usage
    }
}

}