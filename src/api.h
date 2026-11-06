#pragma once
#include <string>

struct ApiResponse {
    int status;
    std::string body;
};
ApiResponse handleRunRequest(const std::string& requestBody);