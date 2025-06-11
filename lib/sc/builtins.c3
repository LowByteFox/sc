module sc::builtins;

import sc;
import std::io;

CFunction[] fns @private = {
    { "+", &add, false },
    { "begin", &begin, false },
    { "display", &display, false },
    { "lambda", &lambda, true },
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

fn Value lambda(Context *ctx, Value[] args) @private {
    if (args.len != 2) return sc::SC_NIL;
    Value lmbd;
    lmbd.type = LAMBDA;
    lmbd.value.lambda.args.init(ctx.allocator);

    ushort ip = args[0].value.lazy;
    usz old_ip = ctx.vm.ip;
    defer ctx.vm.ip = old_ip;
    ip++; /* skip call */
    ctx.vm.ip = ip;
    Instr instr = ctx.vm.get_instr();
    if (instr != ARG_START) {
        Value first_arg = ctx.vm.get_const();
        lmbd.value.lambda.args.push(first_arg.value.symbol);
        ctx.vm.ip++;
        instr = ctx.vm.get_instr();
        do {
            if (instr == ARG_END) break;
            ctx.vm.skip_jump();
            Value arg = ctx.vm.get_const();
            lmbd.value.lambda.args.push(arg.value.symbol);
            instr = ctx.vm.get_instr();
        } while (instr != ARG_END);
    }
    lmbd.value.lambda.body = args[1].value.lazy;
    
    return lmbd;
}

fn Value let(Context *ctx, Value[] args) @private {
    ctx.set_var(ctx.eval_at(args[0].value.lazy, true).value.symbol,
        ctx.eval_at(args[1].value.lazy));
    return sc::sc_num(0);
}

fn void display_val(Value *v, bool in_list) {
    if (v == null || v.type == NOTHING) io::print("nil");
    switch (v.type) {
    case NUMBER: io::print(v.value.num);
    case FLOAT: io::print(v.value.flt);
    case BOOL: io::print(v.value.boolean ? "#t" : "#f");
    case STRING:
        if (in_list) { io::printf("\"%s\"", v.value.str); }
        else { io::print(v.value.str); }
    case SYMBOL: io::print(v.value.symbol);
    case LAZY_ADDR: io::printf("addr(%d)", v.value.lazy);
    case LAMBDA: {
        io::print("λ(");
        io::print(v.value.lambda.args);
        io::printf(";%d) => ...", v.value.lambda.body);
    }
    default:
        io::printf("??? %d!", v.type.ordinal);
    }
}

fn Value display(Context *ctx, Value[] args) {
    if (args.len != 1) return sc::SC_NIL;
    display_val(&args[0], false);
    return sc::SC_NIL;
}
