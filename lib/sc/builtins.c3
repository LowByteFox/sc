module sc::builtins;

import sc;
import std::io;

CFunction[] fns @private = {
    { "+", &add, false },
};

fn Value add(Value[] args) {
    long res = 0;
    foreach (arg : args) {
        res += arg.value.num;
    }
    
    return (Value) { NUMBER, (ValueUnion) { .num = res } };
}
