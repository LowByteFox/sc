module sc::builtins;

import sc;
import std::io;

CFunction[] fns @private = {
    { "+", &add, false },
    { "begin", &begin, false },
    { "let", &let, true },
};

fn Value add(Context *ctx, Value[] args) {
    long res = 0;
    foreach (arg : args) {
        res += arg.value.num;
    }
    
    return sc::sc_num(res);
}

fn Value begin(Context *ctx, Value[] args) => args[args.len - 1];

fn Value let(Context *ctx, Value[] args) {
    ctx.set_var(ctx.eval_at(args[0].value.lazy, true).value.symbol,
        ctx.eval_at(args[1].value.lazy));
    return sc::sc_num(0);
}
