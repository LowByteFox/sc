module sc;
import libc;
import std::io;
import std::os;
import std::core::mem;

extern char *optarg;
extern int optind;
extern fn int getopt(int argc, char** argv, char *optstring);

fn int main(int argc, char **argv)
{
	bool stats;
	ZString eval;
    ZString path;
	int c;

	while ((c = getopt(argc, argv, "hse:f:")) != -1) {
		switch (c) {
		case 'f':
			path = (ZString) optarg;
		case 'e':
			eval = (ZString) optarg;
		case 's':
			stats = true;
		case 'h':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	Allocator alloc = mem;
	Context ctx;
	ctx.init(alloc);
	defer ctx.free();

	if (eval != null && path != null) {
		(void) io::fprintn(io::stderr(), "sc: specify either -e or -f!");
		return 1;
	}

	switch {
	case eval != null:
		ctx.eval(eval.str_view());
	case path != null:
		String content = (String) file::load(mem, path.str_view())!!;
		io::printn(content);
		content.free(allocator::heap());
	default:
		while (true) {
			io::print(">> ");
			io::stdout().flush()!!;

			String in = io::treadline()!!;

			if (in == ".q" || in == ".quit" || in == ".exit") break;
			if (in == ".stats") {}

			ctx.eval(in);
			ctx.free();
			ctx.init(alloc);
		}
	}

	return 0;
}

fn void usage() @private @noreturn {
	(void) io::fprintn(io::stderr(), "usage: sc [-hs] [-e str|-f file]");
	os::exit(1);
}
