module sc;
import libc;
import std::io;
import std::io::file;
import std::os;
import std::core::mem;

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
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

	if (eval != null && path != null) {
		(void) io::fprintn(io::stderr(), "sc: specify either -e or -f!");
		return 1;
	}

	if (eval != null) {
	} else if (path != null) {
		File f = file::open(path.str_view(), "r")!!;
		defer (void) f.close();
		usz off = 0;
		char* content = null;

		while (!f.eof()) {
			content = mem::realloc(content, off + 512);
			/* why read of 513? */
			off += f.read(content[off..off + 512 - 1])!!;
		}
		content[off - 1] = '\0';
		io::printn(((ZString) content).str_view());

		mem::free(content);
	} else {
		while (true) {
			io::print(">> ");
			io::stdout().flush()!!;

			String in = io::treadline()!!;

			if (in == ".q" || in == ".quit" || in == ".exit") break;
			if (in == ".stats") {}

			io::printfn("You typed: %s", in);
		}
	}

	return 0;
}

fn void usage() @private @noreturn {
	(void) io::fprintn(io::stderr(), "usage: sc [-hs] [-e str|-f file]");
	os::exit(1);
}
