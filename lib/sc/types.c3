module sc;
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
	BOOL,
	STRING,
}

union ValueUnion {
	bool boolean;
	long num;
	double flt;
	String str; /* works for symbol as well */
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
