module sc::vm;

import sc;
import sc::builtins @public;
import libc;
import std::io;

macro align(b, a) @private {
	return b % a == 0 ? b : b + a - b % a;
}

fn void Vm.reset_start(&self) {
    self.stack_top = 0;
}

fn void Vm.push(&self, Value val) {
    self.stack.push(val);
    self.stack_top++;
}

fn Value Vm.pop(&self) {
    if (self.stack_top > 0) {
        self.stack_top--;
        return self.stack.pop()!!;
    }

    return sc::SC_NIL;
}

fn void Vm.skip_jump(&self) @private {
    ushort pad = (ushort) align(self.ip + 1, ushort.alignof);
    self.ip = pad + ushort.sizeof;
}

fn ushort Vm.get_jump(&self) @private {
    ushort pad = (ushort) align(self.ip + 1, ushort.alignof);
    ushort *ptr = (void*) self.chunk.instructions.get_ref(pad);
    self.ip = pad + ushort.sizeof;
    return *ptr;
}

fn void interpret(Vm *vm, bool resolve = false) {
    Instr instr = vm.chunk.instructions.get(vm.ip);
    switch (instr) {
    case PUSH_CONST:
        ushort pad = (ushort) align(vm.ip + 1, ushort.alignof);
        ushort *ptr = (void*) vm.chunk.instructions.get_ref(pad);
        Value v = vm.chunk.constants.get(*ptr);
        if (v.type == SYMBOL && !resolve) { vm.push(vm.ctx.get_var(v.value.symbol))!!; }
        else { vm.push(v); }
        vm.ip = pad + ushort.sizeof;
    case CALL:
        usz arg_count = 0;
        vm.ip++;
        interpret(vm, true);
        Value ident = vm.pop();
        String name = ident.value.symbol;
        isz fn_index = -1;
        foreach (isz i, CFunction fun : builtins::fns) {
            if (fun.name == name) {
                fn_index = i;
                break;
            }
        }
        if (fn_index == -1) return;
        bool lazy = builtins::fns[fn_index].lazy;
        vm.ip++;
        instr = vm.chunk.instructions.get(vm.ip);
        do {
            if (instr == ARG_END) break;
            if (lazy) {
                ushort next_ip = vm.get_jump();
                vm.push(sc::sc_lazy((ushort) vm.ip));
                vm.ip = next_ip;
            } else {
                vm.skip_jump();
                interpret(vm);
            }
            arg_count++;
            instr = vm.chunk.instructions.get(vm.ip);
        } while (instr != ARG_END);
        vm.ip++;
        isz start = vm.stack.len() - arg_count;
        Value[] args = vm.stack.array_view()[start:arg_count];
        Value ret = builtins::fns[fn_index].fun(vm.ctx, args);
        foreach (usz i, Value v : args) {
            vm.pop();
        }
        vm.push(ret);
    default:
        io::fprintfn(io::stderr(), "Unhandled instruction \"%s\"!", instr.name);
        sc::disassemble(vm.chunk, "runtime");
        sc::disassembler::print_stack(vm);
        libc::abort();
    }
}
