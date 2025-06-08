module sc::compiler;
import libc;

macro align(b, a) @private {
	return b % a == 0 ? b : b + a - b % a;
}

macro bool is_special(char c) @private => c == '(' || c == ')';

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

fn ushort BytecodeChunk.compile_value(&self, Context *ctx, ushort i) @private {
	Token tok = ctx.tokens.get(i);
	if (tok == Token.NUM ||
		tok == Token.FLOAT ||
		tok == Token.BOOL ||
		tok == Token.STRING ||
		tok == Token.IDENT) self.compile_constant(tok, ctx.code[ctx.locs.get(i)..]);
	if (tok == Token.LPAREN) return self.compile_expr(ctx, i);
	
	return i;
}

fn ushort BytecodeChunk.compile_expr(&self, Context *ctx, ushort begin_i) @private {
	self.instructions.push(Instr.CALL);
	ushort i = begin_i + 1;
	for (; ctx.tokens.get(i) != RPAREN; i++) {
		if (i == begin_i + 1) {
			i = self.compile_value(ctx, i);
			self.instructions.push(Instr.ARG_START);
			continue;
		}
		i = self.compile_value(ctx, i);
	}
	self.instructions.push(Instr.ARG_END);

	return i;
}

fn BytecodeChunk compile(Context *ctx) {
	BytecodeChunk chunk;
	chunk.instructions.init(ctx.allocator);
	chunk.constants.init(ctx.allocator);
	chunk.instructions.push(Instr.RET);
	for (ushort i = 0; ctx.tokens.get(i) != END; i++) {
		i = chunk.compile_value(ctx, i);
	}
	return chunk;
}

fn void BytecodeChunk.compile_constant(&self, Token tok, char[] buf) @private {
	self.instructions.push(Instr.PUSH_CONST);
	Value val;

	switch (tok) {
	case Token.NUM:
		val.type = NUMBER;
		val.value.num = libc::atoll(buf.ptr);
	case Token.FLOAT:
		val.type = FLOAT;
		val.value.flt = libc::strtod(buf.ptr, null);
	case Token.BOOL:
		val.type = BOOL;
		val.value.boolean = buf[0] == 't' ? true : false;
	case Token.STRING:
		val.type = STRING;
		usz size = 0;
		while (buf[size] != '"') { size++; if (buf[size] == '\\') size += 2; }
		val.value.str = (String) buf[0:size];
	case Token.IDENT:
		val.type = SYMBOL;
		usz size = 0;
		while (size < buf.len &&
			!ascii::is_space(buf[size]) && !is_special(buf[size])) size++;
		val.value.symbol = (String) buf[0:size];
	default:
		libc::abort();
	}

	ushort sz = (ushort) self.instructions.len();
	ushort index = (ushort) align(sz, ushort.alignof);
	usz expand = (usz) index - sz + ushort.sizeof;
	self.instructions.reserve(expand);
	self.instructions.force_set_size(self.instructions.len() + expand);
	ushort *ptr = (void*) self.instructions.get_ref(index);
	*ptr = (ushort) self.constants.len();
	self.constants.push(val);
}
