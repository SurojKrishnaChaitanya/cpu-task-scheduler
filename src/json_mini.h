#pragma once
#include <string>
#include <vector>
using namespace std;
namespace mj {

struct Value {
    enum Type { Null, Bool, Int, Real, String, Array, Object };
    Type type = Null;
    bool b = false;
    long long i = 0;                 // Int (saturates at +-LLONG_MAX for huge numbers)
    double d = 0;                    // Real
    string s;                   // String
    vector<Value> arr;          // Array
    vector<string> keys;   // Object: keys[k] belongs to vals[k]
    vector<Value> vals;

    // Object lookup. Returns nullptr if this is not an object or the key is
    // missing. With duplicate keys the LAST one wins.
    const Value* find(const string& key) const;
};

// Parses a complete JSON document. Returns false on any syntax error,
// trailing garbage, or nesting deeper than 32 levels.
bool parse(const string& text, Value& out);

// Returns s as a quoted, escaped JSON string literal.
string quote(const string& s);

}  // namespace mj
