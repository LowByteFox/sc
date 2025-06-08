module sc::compiler;
import libc;

macro align(b, a) @private {
	return b % a == 0 ? b : b + a - b % a;
}

<*
 @require new_size == 0 || self.capacity != 0
*>
fn usz InstrList.force_set_size(&self, usz new_size) @inline @private
{
    usz old_size = self.size;
    self._update_size_change(old_size, new_size);
    self.size = new_size;
    return old_size;
}

fn BytecodeChunk compile(Context *ctx) {
	BytecodeChunk chunk;
	chunk.instructions.init(ctx.allocator);
	chunk.constants.init(ctx.allocator);
	chunk.instructions.push(Instr.RET);
	foreach FOREACH: (ushort i, tok : ctx.tokens) {
		if (tok == END) break;
		if (tok == Token.NUM) chunk.add_constant(tok, ctx.code[ctx.locs.get(i)..]);
	}
	return chunk;
}

fn void BytecodeChunk.add_constant(&self, Token tok, char[] buf) @private {
	self.instructions.push(Instr.PUSH_CONST);

	switch (tok) {
	case Token.NUM:
		long number = libc::atoll(buf.ptr);
		ushort sz = (ushort) self.instructions.len();
		ushort index = (ushort) align(sz, ushort.alignof);
		usz expand = (usz) index - sz + ushort.sizeof;
		self.instructions.reserve(expand);
		self.instructions.force_set_size(self.instructions.len() + expand);
		ushort *ptr = (void*) self.instructions.get_ref(index);
		*ptr = (ushort) self.constants.len();
		self.constants.push((Value) { Type.NUMBER, (ValueUnion) { number } });
	default:
		libc::abort();
	}
}
