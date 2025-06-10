module sc;
import std::collections::list;
import std::collections::linkedlist;
import std::collections::map;

macro align(b, a) @private {
    return b % a == 0 ? b : b + a - b % a;
}

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
    LIST,
    CFUNC,
}

union ValueUnion {
    bool boolean;
    long num;
    double flt;
    String symbol;
    String str;
    ValueList list;
}

struct Value {
    Type type;
    ValueUnion value;
}

enum Instr : char (String name) {
    INVALID =    "Invalid",
    PUSH_CONST = "Constant",
    JMP =        "Jump",
    CALL =       "Call",
    ARG_START =  "Args Start",
    ARG_END =    "Args End",
}

alias TokenList = List{ Token };
alias LocList = List{ ushort };
alias InstrList = List{ Instr };
alias ValueArr = List{ Value };
alias ValueStore = HashMap{ String, Value };
alias ValueList = LinkedList{ Value };
alias Function = fn Value(Value[]);

struct BytecodeChunk {
    InstrList instructions;
    ValueArr constants;
}

struct Environment {
    ValueStore store;
    Environment *parent;
}

struct CFunction {
    String name;
    Function fun;
    bool lazy;
}

struct Vm {
    Allocator allocator;
    BytecodeChunk *chunk;
    ValueArr stack;
    usz stack_top;
    usz ip;
    Environment *current_env;
}

struct Context {
    Allocator allocator;
    TokenList tokens;
    LocList locs;
    char[] code;
    Vm vm;
}

const Value SC_NIL = (Value) {};

<*
@require new_size == 0 || self.capacity != 0
*>
fn usz InstrList.force_set_size(&self, usz new_size) @inline @private {
    usz old_size = self.size;
    self._update_size_change(old_size, new_size);
    self.size = new_size;
    return old_size;
}

fn usz InstrList.alloc_ushort(&self) {
    ushort sz = (ushort) self.len();
    ushort index = (ushort) align(sz, ushort.alignof);
    usz expand = (usz) index - sz + ushort.sizeof;
    self.reserve(expand);
    self.force_set_size(self.len() + expand);
    return index;
}
