module sc::debug;

import std::io;

<* 
	@require chunk.constants.size > index
*>
fn void print_constant(BytecodeChunk *chunk, ushort index) {
	print_value(chunk.constants.get(index));
}

fn void print_value(Value val) {
	switch (val.type) {
	case NUMBER:
		io::printfn("%d", val.value.num);
	default:
		io::printn("nil");
	}
}
