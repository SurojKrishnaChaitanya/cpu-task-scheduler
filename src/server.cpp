#include <cstdio>
#include <cstdlib>
#include <string>
#include "httplib.h"
#include "api.h"

static std::string envOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::atoi(argv[1]) : std::atoi(envOr("PORT", "8080").c_str());
    std::string webDir = (argc > 2) ? argv[2] : "web";
    std::string host = envOr("HOST", std::getenv("PORT") ? "0.0.0.0" : "127.0.0.1");
    std::string origin = envOr("ALLOWED_ORIGIN", "*");
    if (port < 1 || port > 65535) { std::fprintf(stderr, "Invalid port\n"); return 1; }

    httplib::Server svr;
    svr.set_payload_max_length(1 << 20);   // 1 MB is far more than any valid request

    // CORS, so a frontend hosted on another site (for example Vercel) may call this API.
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
        std::printf("Serving frontend from '%s/'\n", webDir.c_str());
    else
        std::printf("No '%s' folder found: API only\n", webDir.c_str());

    std::printf("Listening on http://%s:%d  (POST /run, GET /health), CORS origin: %s\n",
                host.c_str(), port, origin.c_str());
    std::fflush(stdout);
    if (!svr.listen(host, port)) {
        std::fprintf(stderr, "Could not listen on %s:%d (already in use?)\n", host.c_str(), port);
        return 1;
    }
    return 0;
}
