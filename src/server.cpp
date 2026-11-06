#include <cstdio>
#include <cstdlib>
#include <string>
#include "httplib.h"
#include "api.h"
using namespace std;

static string envOr(const char* name, const string& fallback) {
    const char* v = getenv(name);
    return (v && *v) ? string(v) : fallback;
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? atoi(argv[1]) : atoi(envOr("PORT", "8080").c_str());
    string webDir = (argc > 2) ? argv[2] : "web";
    string host = envOr("HOST", getenv("PORT") ? "0.0.0.0" : "127.0.0.1");
    string origin = envOr("ALLOWED_ORIGIN", "*");
    if (port < 1 || port > 65535) { fprintf(stderr, "Invalid port\n"); return 1; }

    httplib::Server svr;
    svr.set_payload_max_length(1 << 20);

    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  origin},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Vary", "Origin"}
    });
    svr.Options("/run", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    svr.Post("/run", [](const httplib::Request& req, httplib::Response& res) {
        ApiResponse r = handleRunRequest(req.body);
        res.status = r.status;
        res.set_content(r.body, "application/json");
    });

    if (svr.set_mount_point("/", webDir))
        printf("Serving frontend from '%s/'\n", webDir.c_str());
    else
        printf("No '%s' folder found: API only\n", webDir.c_str());

    printf("Listening on http://%s:%d  (POST /run, GET /health), CORS origin: %s\n",
                host.c_str(), port, origin.c_str());
    fflush(stdout);
    if (!svr.listen(host, port)) {
        fprintf(stderr, "Could not listen on %s:%d (already in use?)\n", host.c_str(), port);
        return 1;
    }
    return 0;
}