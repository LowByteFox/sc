module sc;

import std::io;
import std::core::mem;
import std::ascii;

fn void Context.init(&self, Allocator allocator) {
	self.allocator = allocator;
	self.tokens.init(self.allocator);
	self.locs.init(self.allocator);
    self.vm.ctx = self;
    self.vm.current_env = mem::allocator::new(allocator, Environment);
    self.vm.current_env.store.init(allocator);
}

fn void Context.free(&self) {
	self.tokens.free();
	self.locs.free();
}

macro bool is_special(char c) @private => c == '(' || c == ')';

fn void Context.set_var(&self, String name, Value val) {
    self.vm.current_env.store[name] = val;
}

fn Value? Context.get_var(&self, String name) => self.vm.current_env.store[name];

fn Value Context.eval_at(&self, ushort ip, bool lazy = false) {
    usz old = self.vm.ip;
    defer self.vm.ip = old;
    self.vm.ip = ip;

    sc::vm::interpret(&self.vm, lazy);
    return self.vm.pop();
}

fn void Context.eval(&self, char[] code) {
	self.code = code;

	for (ushort i = 0; i < code.len; i++) {
		char c = code[i];
		if (ascii::is_space(c)) continue;

		if (c == ';') {
			while (i < code.len && code[i] != '\n') i++; continue;
		}

		if (is_special(c)) {
			self.tokens.push(c == '(' ? LPAREN : RPAREN);
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
			self.tokens.push(parsed_float ? FLOAT : NUM);
			self.locs.push(saved_i);
			continue;
		}

		if (c == '#' && (code[i + 1] == 't' || code[i + 1] == 'f')) {
			i++; self.tokens.push(BOOL); self.locs.push(i);
			continue;
		} else if (c == '"') {
			self.tokens.push(STRING); self.locs.push(i + 1);
			i++;
			while (code[i] != '"') { i++; if (code[i] == '\\') i += 2; }
			continue;
		}

		self.tokens.push(IDENT);
		self.locs.push(i);
		while (i + 1 < code.len &&
			!ascii::is_space(code[i + 1]) && !is_special(code[i + 1])) i++;
	}

	self.tokens.push(Token.END);

    BytecodeChunk chunk = compile(self);
    interpret(self, &chunk);
}

fn BytecodeChunk compile(Context *self) {
	return sc::compiler::compile(self);
}

fn Value interpret(Context *self, BytecodeChunk *chunk) {
    self.vm.chunk = chunk;
    self.vm.ip = 0;

    while (self.vm.ip < chunk.instructions.len()) {
        sc::vm::interpret(&self.vm);
    }

    sc::disassembler::print_stack(&self.vm);

    return self.vm.pop();
}

fn void disassemble(BytecodeChunk *chunk, ZString name) @export("sc_disassemble") {
	sc::disassembler::disassemble(chunk, name.str_view());
}
