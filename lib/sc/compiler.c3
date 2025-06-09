module sc::compiler;
import libc;

macro bool is_special(char c) @private => c == '(' || c == ')';

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
    bool added_arg_start = false;
    for (; ctx.tokens.get(i) != RPAREN; i++) {
        if (i == begin_i + 1) {
            i = self.compile_value(ctx, i);
            self.instructions.push(Instr.ARG_START);
            added_arg_start = true;
            continue;
        }

        self.instructions.push(Instr.JMP);
        usz index = self.instructions.alloc_ushort();
        i = self.compile_value(ctx, i);
        ushort *where_to = (ushort*) self.instructions.get_ref(index);
        *where_to = (ushort) self.instructions.len();
    }
    if (!added_arg_start) self.instructions.push(Instr.ARG_START);
    self.instructions.push(Instr.ARG_END);

    return i;
}

fn BytecodeChunk compile(Context *ctx) {
    BytecodeChunk chunk;
    chunk.instructions.init(ctx.allocator);
    chunk.constants.init(ctx.allocator);
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

    usz index = self.instructions.alloc_ushort();
    ushort *ptr = (ushort*) self.instructions.get_ref(index);
    *ptr = (ushort) self.constants.len();
    self.constants.push(val);
}
