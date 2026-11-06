#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include "api.h"
#include "json_mini.h"
using namespace std;
using mj::Value;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)
static bool near(double a, double b) { return fabs(a - b) < 1e-9; }

static const Value& get(const Value& v, const char* key) {
    static const Value missing;
    const Value* p = v.find(key);
    return p ? *p : missing;
}
static bool isInt(const Value& v, long long x) { return v.type == Value::Int && v.i == x; }
static double num(const Value& v) { return v.type == Value::Int ? static_cast<double>(v.i) : v.d; }

static Value run(const string& body, int* status = nullptr) {
    ApiResponse r = handleRunRequest(body);
    if (status) *status = r.status;
    Value v;
    if (!mj::parse(r.body, v)) { printf("  FAIL: response is not valid JSON: %s\n", r.body.c_str()); ++failures; }
    return v;
}

static bool ganttIs(const Value& g, initializer_list<array<int, 3>> want) {
    if (g.arr.size() != want.size()) return false;
    size_t i = 0;
    for (const auto& w : want) {
        const Value& e = g.arr[i++];
        const Value& pid = get(e, "pid");
        if (w[0] == -1 ? pid.type != Value::Null : !isInt(pid, w[0])) return false;
        if (!isInt(get(e, "start"), w[1]) || !isInt(get(e, "end"), w[2])) return false;
    }
    return true;
}

static void expectError(const string& body, const char* mustContain) {
    int status = 0;
    Value j = run(body, &status);
    bool ok = status == 400 && get(j, "error").type == Value::String &&
              get(j, "error").s.find(mustContain) != string::npos;
    if (!ok) printf("  (got status %d, error: %s)\n", status, get(j, "error").s.c_str());
    CHECK(ok);
} 

static const char* BASIC_PROCS =
    R"([{"pid":1,"arrival":0,"burst":5,"priority":1},
        {"pid":2,"arrival":1,"burst":3,"priority":1},
        {"pid":3,"arrival":2,"burst":1,"priority":1}])";

static const char* PRIO_PROCS =
    R"([{"pid":1,"arrival":0,"burst":4,"priority":2},
        {"pid":2,"arrival":1,"burst":3,"priority":4},
        {"pid":3,"arrival":2,"burst":1,"priority":3},
        {"pid":4,"arrival":3,"burst":2,"priority":1}])";

static void testParserValid() {
    printf("Test: parser accepts valid JSON\n");
    Value v;
    CHECK(mj::parse(R"( {"a": [1, -2, 3.5, 1e2, true, false, null, "x\n\"y\u0041"], "b": {"c": {}}} )", v));
    CHECK(v.type == Value::Object);
    const Value& a = get(v, "a");
    CHECK(a.arr.size() == 8);
    CHECK(isInt(a.arr[0], 1) && isInt(a.arr[1], -2));
    CHECK(a.arr[2].type == Value::Real && near(a.arr[2].d, 3.5));
    CHECK(a.arr[3].type == Value::Real && near(a.arr[3].d, 100));
    CHECK(a.arr[4].type == Value::Bool && a.arr[4].b && !a.arr[5].b);
    CHECK(a.arr[6].type == Value::Null);
    CHECK(a.arr[7].s == "x\n\"yA");
    CHECK(get(get(v, "b"), "c").type == Value::Object);

    CHECK(mj::parse("123456789012345678901234567890", v) && v.type == Value::Int && v.i == LLONG_MAX);
    CHECK(mj::parse(R"({"a":1,"a":2})", v) && isInt(get(v, "a"), 2));   // last duplicate wins
    CHECK(mj::parse("[]", v) && v.arr.empty());
    string nested(20, '['); nested += string(20, ']');
    CHECK(mj::parse(nested, v));
}

static void testParserInvalid() {
    printf("Test: parser rejects invalid JSON\n");
    const char* bad[] = {"", "   ", "{", "[1,]", "{\"a\":1,}", "{a:1}", "[1] x", "\"abc", "01", "-",
                         "1.", "1e", "tru", "[1 2]", "{\"a\" 1}", "\"\\q\"", "\"\\u12G4\"", "\"a\nb\"",
                         "{\"a\":}", "nul", "'x'"};
    Value v;
    for (const char* b : bad) {
        bool rejected = !mj::parse(b, v);
        if (!rejected) printf("  (wrongly accepted: %s)\n", b);
        CHECK(rejected);
    }
    string deep(100, '['); deep += string(100, ']');
    CHECK(!mj::parse(deep, v));
}

static void testQuote() {
    printf("Test: quote() escapes output strings\n");
    CHECK(mj::quote("a\"b\\c\n\t\x01") == "\"a\\\"b\\\\c\\n\\t\\u0001\"");
    CHECK(mj::quote("plain") == "\"plain\"");
}

static void testRoundRobinResponse() {
    printf("Test: RR response (hand-worked example)\n");
    int status = 0;
    Value j = run(string(R"({"algorithm":"rr","quantum":2,"processes":)") + BASIC_PROCS + "}", &status);
    CHECK(status == 200);
    CHECK(get(j, "algorithm").s == "rr" && isInt(get(j, "quantum"), 2));
    const Value& g = get(j, "gantt");
    CHECK(ganttIs(g, {{1,0,2},{2,2,4},{3,4,5},{1,5,7},{2,7,8},{1,8,9}}));
    CHECK(get(g.arr[0], "idle").type == Value::Bool && !get(g.arr[0], "idle").b);
    const Value& q = get(g.arr[1], "queue");
    CHECK(q.arr.size() == 2 && isInt(q.arr[0], 3) && isInt(q.arr[1], 1));   // waiting list for the animation
    const Value& rows = get(j, "processes");
    CHECK(rows.arr.size() == 3);
    CHECK(isInt(get(rows.arr[0], "pid"), 1) && isInt(get(rows.arr[0], "completion"), 9) &&
          isInt(get(rows.arr[0], "turnaround"), 9) && isInt(get(rows.arr[0], "waiting"), 4));
    CHECK(isInt(get(rows.arr[2], "completion"), 5) && isInt(get(rows.arr[2], "waiting"), 2));
    CHECK(near(num(get(j, "averageTurnaround")), 19.0 / 3) && near(num(get(j, "averageWaiting")), 10.0 / 3));
    CHECK(isInt(get(j, "totalTime"), 9) && isInt(get(j, "idleTime"), 0));
    CHECK(near(num(get(j, "averageResponse")), 1.0) && isInt(get(rows.arr[2], "response"), 2));
}

static void testIdleSerialization() {
    printf("Test: idle gap appears as pid null, idle true\n");
    Value j = run(R"({"algorithm":"rr","quantum":2,"processes":[
        {"pid":1,"arrival":0,"burst":2},{"pid":2,"arrival":10,"burst":2}]})");
    const Value& g = get(j, "gantt");
    CHECK(ganttIs(g, {{1,0,2},{-1,2,10},{2,10,12}}));
    CHECK(get(g.arr[1], "idle").b && get(g.arr[1], "pid").type == Value::Null);
    CHECK(isInt(get(j, "idleTime"), 8) && isInt(get(j, "totalTime"), 12));
    CHECK(isInt(get(get(j, "processes").arr[0], "priority"), 0));   // optional priority defaults to 0
}

static void testPriorityResponses() {
    printf("Test: priority non-preemptive and preemptive responses\n");
    string procs = PRIO_PROCS;
    Value np = run(R"({"algorithm":"priority","processes":)" + procs + "}");
    CHECK(get(np, "preemptive").type == Value::Bool && !get(np, "preemptive").b);
    CHECK(ganttIs(get(np, "gantt"), {{1,0,4},{2,4,7},{3,7,8},{4,8,10}}));
    CHECK(near(num(get(np, "averageWaiting")), 13.0 / 4) && near(num(get(np, "averageTurnaround")), 23.0 / 4));

    Value pre = run(R"({"algorithm":"priority","preemptive":true,"processes":)" + procs + "}");
    CHECK(get(pre, "preemptive").b);
    CHECK(ganttIs(get(pre, "gantt"), {{1,0,1},{2,1,4},{3,4,5},{1,5,8},{4,8,10}}));
    CHECK(near(num(get(pre, "averageWaiting")), 11.0 / 4) && near(num(get(pre, "averageTurnaround")), 21.0 / 4));
}

static void testPureFunction() {
    printf("Test: same request twice gives byte-identical responses\n");
    string body = string(R"({"algorithm":"rr","quantum":3,"processes":)") + BASIC_PROCS + "}";
    CHECK(handleRunRequest(body).body == handleRunRequest(body).body);
}

static void testBadRequests() {
    printf("Test: bad requests give 400 with a clear message\n");
    string p = BASIC_PROCS;
    expectError("not json at all", "not valid JSON");
    expectError("[1,2,3]", "must be a JSON object");
    expectError(R"({"processes":[]})", "algorithm");
    expectError(R"({"algorithm":"fcfs","processes":[]})", "Unknown algorithm");
    expectError(R"({"algorithm":"rr","processes":)" + p + "}", "quantum");
    expectError(R"({"algorithm":"rr","quantum":0,"processes":)" + p + "}", "at least 1");
    expectError(R"({"algorithm":"rr","quantum":"2","processes":)" + p + "}", "whole number");
    expectError(R"({"algorithm":"rr","quantum":2})", "processes");
    expectError(R"({"algorithm":"rr","quantum":2,"processes":[]})", "At least one process");
    expectError(R"({"algorithm":"priority","preemptive":"yes","processes":)" + p + "}", "preemptive");
}

static void testBadProcesses() {
    printf("Test: bad process rows name the row and the problem\n");
    auto rr = [](const string& procs) {
        return R"({"algorithm":"rr","quantum":2,"processes":)" + procs + "}";
    };
    expectError(rr(R"([{"pid":1,"arrival":0,"burst":0}])"), "Process 1: Burst time must be at least 1");
    expectError(rr(R"([{"pid":1,"arrival":-1,"burst":2}])"), "Arrival time must not be negative");
    expectError(rr(R"([{"pid":1,"arrival":0,"burst":2},{"pid":1,"arrival":1,"burst":2}])"), "Process 2: Duplicate PID");
    expectError(rr(R"([{"pid":1,"arrival":0,"burst":2.5}])"), "'burst' must be a whole number");
    expectError(rr(R"([{"pid":1,"arrival":0}])"), "Missing field 'burst'");
    expectError(rr(R"([5])"), "must be an object");
    expectError(rr(R"([{"pid":1,"arrival":0,"burst":2,"priority":"high"}])"), "'priority' must be a whole number");
    expectError(rr(R"([{"pid":1,"arrival":0,"burst":99999999}])"), "out of range");
    expectError(rr(R"([{"pid":1,"arrival":0,"burst":123456789012345678901234567890}])"), "out of range");
}

static void testLimits() {
    printf("Test: size limits\n");
    ostringstream many;
    many << R"({"algorithm":"rr","quantum":1,"processes":[)";
    for (int i = 0; i < 501; ++i) many << (i ? "," : "") << "{\"pid\":" << i << ",\"arrival\":0,\"burst\":1}";
    many << "]}";
    expectError(many.str(), "Too many processes");

    expectError(R"({"algorithm":"priority","processes":[
        {"pid":1,"arrival":0,"burst":40000},{"pid":2,"arrival":0,"burst":40000},
        {"pid":3,"arrival":0,"burst":40000}]})", "Total burst time is too large");
}

int main() {
    testParserValid();
    testParserInvalid();
    testQuote();
    testRoundRobinResponse();
    testIdleSerialization();
    testPriorityResponses();
    testPureFunction();
    testBadRequests();
    testBadProcesses();
    testLimits();
    if (failures == 0) printf("\nAll API tests passed.\n");
    else printf("\n%d check(s) FAILED.\n", failures);
    return failures ? 1 : 0;
}