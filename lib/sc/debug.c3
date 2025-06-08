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
	case FLOAT:
		io::printfn("%f", val.value.flt);
	case BOOL:
		io::printfn("%s", val.value.boolean ? "#t" : "#f");
	case STRING:
		io::printfn("\"%s\"", val.value.str);
	case SYMBOL:
		io::printfn("%s", val.value.symbol);
	default:
		io::printn("nil");
	}
}
