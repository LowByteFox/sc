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
	SYMBOL,
}

union ValueUnion {
	bool boolean;
	long num;
	double flt;
	String symbol;
	String str;
}

struct Value {
	Type type;
	ValueUnion value;
}

enum Instr : char (String name) {
	INVALID =    "Invalid",
	RET =        "Return",
	PUSH_CONST = "Constant",
	CALL =       "Call",
	ARG_START =  "Args Start",
	ARG_END =    "Args End",
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
