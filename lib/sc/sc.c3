module sc;

import std::io;
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

alias TokenList = List{ Token };
alias LocList = List{ ushort };

struct Context {
	Allocator allocator;
	TokenList tokens;
	LocList locs;
	char[] code;
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
				while (ascii::is_digit(code[i + 1])) i++;
				if (parsed_float) break;
				if (code[i + 1] == '.') { parsed_float = true; i++; }
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

	for (ushort i = 0; self.tokens.get(i) != Token.END; i++) {
		io::printfn("%3d | %s", self.locs.get(i), self.tokens.get(i).description);
	}
}
