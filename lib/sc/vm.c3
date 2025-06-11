module sc::vm;

import sc;
import sc::builtins @public;
import libc;
import std::io;
import std::core::mem;

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

fn void Vm.push_frame(&self) {
    Environment *env = mem::allocator::new(self.ctx.allocator, Environment);
    env.store.init(self.ctx.allocator);
    env.parent = self.current_env;
    self.current_env = env;
}

fn void Vm.pop_frame(&self) {
    Environment *env = self.current_env;
    if (env.parent == null) return;
    self.current_env = env.parent;
    env.store.free();
    mem::allocator::free(self.ctx.allocator, env);
}

fn void Vm.skip_jump(&self) {
    ushort pad = (ushort) align(self.ip + 1, ushort.alignof);
    self.ip = pad + ushort.sizeof;
}

fn Instr Vm.get_instr(&self) => self.chunk.instructions.get(self.ip);

fn Value Vm.get_const(&self) {
    ushort pad = (ushort) align(self.ip + 1, ushort.alignof);
    ushort *ptr = (void*) self.chunk.instructions.get_ref(pad);
    Value v = self.chunk.constants.get(*ptr);
    self.ip = pad + ushort.sizeof;
    return v;
}

fn ushort Vm.get_jump(&self) @private {
    ushort pad = (ushort) align(self.ip + 1, ushort.alignof);
    ushort *ptr = (void*) self.chunk.instructions.get_ref(pad);
    self.ip = pad + ushort.sizeof;
    return *ptr;
}

fn Value Vm.interpret_lambda(&self, Value *lambda, Value[] args) {
    self.push_frame();
    defer self.pop_frame();
    foreach (ushort i, String arg : lambda.value.lambda.args) {
        self.ctx.set_var(arg, args[i]);
    }
    usz old_ip = self.ip;
    defer self.ip = old_ip;
    self.ip = lambda.value.lambda.body;
    interpret(self);
    return self.pop();
}

fn void interpret(Vm *vm, bool resolve = false) {
    Instr instr = vm.chunk.instructions.get(vm.ip);
    switch (instr) {
    case PUSH_CONST:
        Value v = vm.get_const();
        if (v.type == SYMBOL && !resolve) { vm.push(vm.ctx.get_var(v.value.symbol))!!; }
        else { vm.push(v); }
    case CALL:
        usz arg_count = 0;
        vm.ip++;
        interpret(vm, true);
        Value ident = vm.pop();
        isz fn_index = -1;
        bool lazy = false;
        if (ident.type == SYMBOL) {
            String name = ident.value.symbol;
            foreach (isz i, CFunction fun : builtins::fns) {
                if (fun.name == name) {
                    fn_index = i;
                    break;
                }
            }
            if (fn_index == -1) return;
            lazy = builtins::fns[fn_index].lazy;
        } /* let's pretend there is lambda */
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
        if (fn_index != -1) {
            Value ret = builtins::fns[fn_index].fun(vm.ctx, args);
            foreach (usz i, Value v : args) {
                vm.pop();
            }
            vm.push(ret);
        } else {
            Value ret = vm.interpret_lambda(&ident, args);
            foreach (usz i, Value v : args) {
                vm.pop();
            }
            vm.push(ret);
        }
    default:
        io::fprintfn(io::stderr(), "Unhandled instruction \"%s\"!", instr.name);
        sc::disassemble(vm.chunk, "runtime");
        sc::disassembler::print_stack(vm);
        libc::abort();
    }
}
