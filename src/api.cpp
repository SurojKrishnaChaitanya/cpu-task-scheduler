#include "api.h"
#include <string>
#include "json_mini.h"
#include "process.h"
#include "scheduler.h"
#include "serializer.h"
#include "stats.h"
using namespace std;
using mj::Value;

static const int  MAX_PROCESSES   = 500;
static const long MAX_TOTAL_BURST = 100000;
static const long MAX_VALUE       = 1000000;

static ApiResponse fail(const string& message) {
    return {400, "{\"error\":" + mj::quote(message) + "}"};
}

static string readInt(const Value& obj, const char* key, bool required, int* out) {
    const Value* v = obj.find(key);
    if (!v || v->type == Value::Null) {
        if (required) return string("Missing field '") + key + "'";
        return "";
    }
    if (v->type != Value::Int) return string("Field '") + key + "' must be a whole number";
    if (v->i < -MAX_VALUE || v->i > MAX_VALUE) return string("Field '") + key + "' is out of range";
    *out = static_cast<int>(v->i);
    return "";
}

ApiResponse handleRunRequest(const string& requestBody) {
    Value req;
    if (!mj::parse(requestBody, req)) return fail("Request body is not valid JSON");
    if (req.type != Value::Object)    return fail("Request body must be a JSON object");

    // ---- algorithm and its options
    const Value* algoField = req.find("algorithm");
    if (!algoField || algoField->type != Value::String)
        return fail("Missing field 'algorithm' (\"rr\" or \"priority\")");
    const string algorithm = algoField->s;
    if (algorithm != "rr" && algorithm != "priority")
        return fail("Unknown algorithm '" + algorithm + "' (use \"rr\" or \"priority\")");

    int quantum = 0;
    bool preemptive = false;
    if (algorithm == "rr") {
        string err = readInt(req, "quantum", true, &quantum);
        if (!err.empty()) return fail(err);
        if (quantum < 1) return fail("Time quantum must be at least 1");
    } else {
        const Value* pre = req.find("preemptive");
        if (pre && pre->type != Value::Null) {
            if (pre->type != Value::Bool) return fail("Field 'preemptive' must be true or false");
            preemptive = pre->b;
        }
    }

    const Value* procs = req.find("processes");
    if (!procs || procs->type != Value::Array) return fail("Missing field 'processes' (a list)");
    if (procs->arr.empty()) return fail("At least one process is required");
    if (procs->arr.size() > static_cast<size_t>(MAX_PROCESSES))
        return fail("Too many processes (maximum " + to_string(MAX_PROCESSES) + ")");

    ProcessList list;
    long totalBurst = 0;
    for (size_t i = 0; i < procs->arr.size(); ++i) {
        const string who = "Process " + to_string(i + 1) + ": ";
        const Value& item = procs->arr[i];
        if (item.type != Value::Object) return fail(who + "must be an object");

        int pid = 0, arrival = 0, burst = 0, priority = 0;
        string err;
        if (!(err = readInt(item, "pid",      true,  &pid)).empty())      return fail(who + err);
        if (!(err = readInt(item, "arrival",  true,  &arrival)).empty())  return fail(who + err);
        if (!(err = readInt(item, "burst",    true,  &burst)).empty())    return fail(who + err);
        if (!(err = readInt(item, "priority", false, &priority)).empty()) return fail(who + err);

        if (!list.append(pid, arrival, burst, priority, &err)) return fail(who + err);

        totalBurst += burst;
        if (totalBurst > MAX_TOTAL_BURST)
            return fail("Total burst time is too large (maximum " + to_string(MAX_TOTAL_BURST) + ")");
    }

    ScheduleResult result = (algorithm == "rr") ? roundRobin(list, quantum)
                                                : priorityScheduling(list, preemptive);
    if (!result.ok()) return fail(result.error);
    Stats stats = computeStats(list);

    RunInfo info;
    info.algorithm = algorithm;
    info.quantum = quantum;
    info.preemptive = preemptive;
    return {200, serializeResult(list, result, stats, info)};
}