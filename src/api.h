#pragma once
#include <string>
using namespace std;

struct ApiResponse {
    int status;
    string body;
};
ApiResponse handleRunRequest(const std::string& requestBody);