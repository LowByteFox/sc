module sc::vm;

import sc;
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

fn void interpret(Vm *vm) {
    Instr instr = vm.chunk.instructions.get(vm.ip);
    switch (instr) {
    case PUSH_CONST:
        ushort pad = (ushort) align(vm.ip + 1, ushort.alignof);
        ushort *ptr = (void*) vm.chunk.instructions.get_ref(pad);
        vm.push(vm.chunk.constants.get(*ptr));
        vm.ip = pad + ushort.sizeof;
    default:
        io::fprintfn(io::stderr(), "Unhandled instruction \"%s\"!", instr.name);
        sc::disassemble(vm.chunk, "runtime");
        sc::disassembler::print_stack(vm);
        libc::abort();
    }
}
