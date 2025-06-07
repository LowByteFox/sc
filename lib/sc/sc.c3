module sc;

import std::io;
import libc;
import std::core::mem;
import std::ascii;
import std::collections::list;

enum Token : char (String description) {
	END = "End",
	LPAREN = "(",
	RPAREN = ")",
	IDENT = "Identifier",
	NUM = "Number",
	FLOAT = "Float",
	BOOL = "Bool",
	STRING = "String",
}

enum Type : char {
	NOTHING,
	NUMBER,
	FLOAT,
}

union ValueUnion {
	long num;
	double flt;
}

struct Value {
	Type type;
	ValueUnion value;
}

enum Instr : char (String name) {
	INVALID = "Invalid",
	RET = "Return",
	PUSH_CONST = "Constant",
}

alias TokenList = List{ Token };
alias LocList = List{ ushort };
alias InstrList = List{ Instr };
alias ValueList = List{ Value };

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

struct BytecodeChunk {
	InstrList instructions;
	ValueList constants;
}

struct Context {
	Allocator allocator;
	TokenList tokens;
	LocList locs;
	char[] code;
}

macro align(b, a) @private {
	return b % a == 0 ? b : b + a - b % a;
}

fn void Context.init(&self, Allocator allocator) {
	self.allocator = allocator;
	self.tokens.init(self.allocator);
	self.locs.init(self.allocator);
}

fn void Context.deinit(&self) {
	self.tokens.free();
	self.locs.free();
}

macro bool is_special(char c) @private => c == '(' || c == ')';

fn void Context.eval(&self, char[] code) {
	self.code = code;

	for (ushort i = 0; i < code.len; i++) {
		char c = code[i];
		if (ascii::is_space(c)) continue;

		if (c == ';') {
			while (i < code.len && code[i] != '\n') i++; continue;
		}

		if (is_special(c)) {
			self.tokens.push(c == '(' ? Token.LPAREN : Token.RPAREN);
			self.locs.push(i);
			continue;
		}

		if IDENT: (ascii::is_digit(c) || c == '-') {
			if (c == '-' && !ascii::is_digit(code[i + 1])) break IDENT;
			ushort saved_i = i; bool parsed_float = false;
			do {
				while (i + 1 < code.len && ascii::is_digit(code[i + 1])) i++;
				if (parsed_float) break;
				if (i + 1 < code.len && code[i + 1] == '.') { parsed_float = true; i++; }
				else { break; }
			} while(true);
			self.tokens.push(parsed_float ? Token.FLOAT : Token.NUM);
			self.locs.push(saved_i);
			continue;
		}

		if (c == '#' && (code[i + 1] == 't' || code[i + 1] == 'f')) {
			i++; self.tokens.push(Token.BOOL); self.locs.push(i);
			continue;
		} else if (c == '"') {
			self.tokens.push(Token.STRING); self.locs.push(i + 1);
			i++;
			while (code[i] != '"') { i++; if (code[i] == '\\') i += 2; }
			continue;
		}

		self.tokens.push(Token.IDENT);
		self.locs.push(i);
		while (i + 1 < code.len &&
			!ascii::is_space(code[i + 1]) && !is_special(code[i + 1])) i++;
	}

	self.tokens.push(Token.END);

	// for (ushort i = 0; self.tokens.get(i) != Token.END; i++) {
	// 	io::printfn("%3d | %s", self.locs.get(i), self.tokens.get(i).description);
	// }

	BytecodeChunk chunk = self.compile();
	chunk.disassemble("Main");
}

fn BytecodeChunk Context.compile(&self) @private {
	BytecodeChunk chunk;
	chunk.instructions.init(self.allocator);
	chunk.constants.init(self.allocator);
	chunk.instructions.push(Instr.RET);
	for (ushort i = 0; self.tokens.get(i) != Token.END; i++) {
		Token tok = self.tokens.get(i);
		if (tok == Token.NUM) chunk.add_constant(tok, self.code[self.locs.get(i)..]);
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

fn void BytecodeChunk.disassemble(&self, String name) @private {
	io::printfn("== Dissasemble [%s] ==", name);
	for (ushort i = 0; i < self.instructions.len(); i++) {
		Instr instr = self.instructions.get(i);
		switch (instr) {
		case RET:
			io::printfn("%4d | %s", i, instr.name);
		case PUSH_CONST:
			ushort pad = (ushort) align(i + 1, ushort.alignof);
			ushort *ptr = (ushort*) (self.instructions.entries + pad);
			io::printf("%4d | %-16s %6d => ", i, instr.name, *ptr);
			self.print_value(*ptr);
			i = pad + ushort.sizeof - 1;
		default:
			io::printfn("Unhandled instruction %d!", instr.ordinal);
		}
	}
}

fn void BytecodeChunk.print_value(&self, ushort index) {
	Value v = self.constants.get(index);
	switch (v.type) {
	case NUMBER:
		io::printfn("%d", v.value.num);
	default:
		io::printn("nil");
	}
}
