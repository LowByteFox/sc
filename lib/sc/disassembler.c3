module sc::disassembler;

import std::io;
import sc::debug;

macro align(b, a) @private {
	return b % a == 0 ? b : b + a - b % a;
}

fn void disassemble(BytecodeChunk *chunk, String name) {
	io::printfn("== Dissasemble [%s] ==", name);
	for (ushort i = 0; i < chunk.instructions.len(); i++) {
		Instr instr = chunk.instructions.get(i);
		switch (instr) {
		case CALL:
		case ARG_START:
		case ARG_END:
			io::printfn("%4d | %s", i, instr.name);
        case JMP:
            ushort pad = (ushort) align(i + 1, ushort.alignof);
            ushort *ptr = (void*) chunk.instructions.get_ref(pad);
			io::printfn("%4d | %-12s %6d", i, instr.name, *ptr);
			i = pad + ushort.sizeof - 1;
		case PUSH_CONST:
			ushort pad = (ushort) align(i + 1, ushort.alignof);
			ushort *ptr = (ushort*) chunk.instructions.get_ref(pad);
			io::printf("%4d | %-12s %6d => ", i, instr.name, *ptr);
			debug::print_constant(chunk, *ptr);
			i = pad + ushort.sizeof - 1;
		default:
			io::printfn("Unhandled instruction %d!", instr.ordinal);
		}
	}
}

fn void print_stack(Vm *vm) {
    io::printn("== Stack ==");
    for (ushort i = 0; i < vm.stack_top; i++) {
        io::printf("%4d | ", i);
        debug::print_value(vm.stack.get(i));
    }
}
