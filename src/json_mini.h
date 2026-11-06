#pragma once
#include <string>
#include <vector>
using namespace std;

namespace mj {
struct Value {
    enum Type { Null, Bool, Int, Real, String, Array, Object };
    Type type = Null;
    bool b = false;
    long long i = 0;      
    double d = 0;                  
    string s;                 
    vector<Value> arr;          
    vector<string> keys; 
    vector<Value> vals;

    const Value* find(const string& key) const;
};

bool parse(const string& text, Value& out);
string quote(const string& s);
}