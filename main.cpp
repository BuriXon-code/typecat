/*
 * Author: Kamil BuriXon Burek
 * Name: typecat
 * Version: 1.0
 * Year: 2026
 * Description:
 *	 typecat is a terminal-based text display tool that simulates typing
 *	 text from files, stdin, or provided strings. It supports configurable
 *	 typing speed, error simulation, line numbering, color/escape sequences,
 *	 and audible beeps on errors. It also handles standard POSIX signals
 *	 gracefully and provides detailed exit codes for debugging and scripting.
 * License: GPL v3.0
 *	 This program is free software: you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation, either version 3 of the License, or
 *	 (at your option) any later version.
 *	 The GPL v3.0 ensures that the software remains free, requires
 *	 disclosure of source when distributing binaries, and preserves
 *	 user freedoms to run, modify, and share the software.
 */
 
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <errno.h>
#include <cmath>
#include <cstring>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>

using namespace std;

int speed = 50;
bool mistakes = false;
bool stdin_mode = false;
string file_input = "";
bool escapes = false;
bool print_escapes = false;
int TABSIZE = 8;
int MISTAKE_CHANCE = 10;
vector<string> texts;
bool show_all = false;
bool line_numbers = false;
bool input_is_binary = false;
bool allow_resize = false;
bool debug_enabled = false;
bool beep_on_error = false;

std::mt19937 rng((unsigned)chrono::high_resolution_clock::now().time_since_epoch().count());

unordered_map<string,string> neigh = {
	{"a","qwsz"}, {"b","vghn"}, {"c","xdfv"}, {"d","ersfcx"}, {"e","wsdr"}, {"f","drtgvc"},
	{"g","ftyhbv"}, {"h","gyujnb"}, {"i","ujko"}, {"j","huikmn"}, {"k","jiolm"}, {"l","kop"},
	{"m","njk"}, {"n","bhjm"}, {"o","iklp"}, {"p","ol"}, {"q","wa"}, {"r","edft"},
	{"s","awedxz"}, {"t","rfgy"}, {"u","yhji"}, {"v","cfgb"}, {"w","qase"}, {"x","zsdc"},
	{"y","tghu"}, {"z","asx"},
	{"1","2q"}, {"2","13w"}, {"3","24e"}, {"4","35r"}, {"5","46t"}, {"6","57y"}, {"7","68u"},
	{"8","79i"}, {"9","80o"}, {"0","9p"},
	{",","m.<>"}, {".",">,/l"}, {"/",".?;"}, {"\\","|"}, {"|","\\"}, {";","lk'"},
	{":","L\""}, {"'",";\""}, {"\"",";'"}, {"[","p-]=\\;"}, {"]","[\\'"},
	{"{","P_+}]"}, {"}","[{\\|"}, {"=","+-"}, {"+","=-"}, {"-","=_"}, {"_","-"},
	{"(","9"}, {")","0"}, {"*","8"}, {"&","67"}, {"^","45"}, {"%","45"},
	{"$","34"}, {"#","23"}, {"@","12"}, {"!","12"}, {"~","`"}, {"`","~"}
};

volatile sig_atomic_t sig_flag = 0;
static int sig_pipe_fds[2] = {-1, -1};

int get_cols(){
	struct winsize w;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) return 80;
	if(w.ws_col == 0) return 80;
	return (int)w.ws_col;
}

double calc_delay(){
	int value = 100 - speed;
	int max_r = value > 0 ? value - 1 : 0;
	uniform_int_distribution<int> dist(0, max_r);
	int chosen = dist(rng);
	int finalv = value + chosen;
	if(finalv < 1) finalv = 1;
	return (double)finalv / 1500.0;
}

string basename_of(const string &path){
	size_t p = path.find_last_of("/\\");
	if(p == string::npos) return path;
	return path.substr(p+1);
}

bool looks_binary(const string &data){
	if(data.empty()) return false;
	size_t sample = min((size_t)4096, data.size());
	size_t nonprint = 0;
	for(size_t i=0;i<sample;++i){
		unsigned char c = (unsigned char)data[i];
		if(c == 0) return true;
		if(c < 0x09) { nonprint++; continue; }
		if(c > 0x0D && c < 0x20) { nonprint++; continue; }
	}
	double frac = (double)nonprint / (double)sample;
	return frac > 0.30;
}

void replace_all(string &s, const string &from, const string &to){
	if(from.empty()) return;
	size_t pos = 0;
	while((pos = s.find(from, pos)) != string::npos){
		s.replace(pos, from.size(), to);
		pos += to.size();
	}
}

string strip_ansi(const string &s){
	string out;
	out.reserve(s.size());
	size_t i = 0, n = s.size();
	while(i < n){
		unsigned char ch = (unsigned char)s[i];
		if(ch == 0x1B){
			++i;
			if(i < n && s[i] == '['){
				++i;
				while(i < n){
					unsigned char cc = (unsigned char)s[i++];
					if(cc >= 0x40 && cc <= 0x7E) break;
				}
				continue;
			}
			if(i < n && s[i] == ']'){
				++i;
				while(i < n){
					if(s[i] == '\a'){ ++i; break; }
					if(s[i] == 0x1B && (i+1) < n && s[i+1] == '\\'){ i += 2; break; }
					++i;
				}
				continue;
			}
			if(i < n) { ++i; continue; }
			continue;
		}
		out.push_back((char)ch);
		++i;
	}
	return out;
}

string render_escapes_as_text(const string &s){
	string out;
	out.reserve(s.size() * 2);
	size_t i = 0, n = s.size();
	while(i < n){
		unsigned char ch = (unsigned char)s[i];
		if(ch == 0x1B){
			if(i + 1 < n){
				unsigned char next = (unsigned char)s[i+1];
				if(next == '['){
					size_t j = i + 2;
					string content;
					while(j < n){
						unsigned char c = (unsigned char)s[j++];
						content.push_back((char)c);
						if(c >= 0x40 && c <= 0x7E) break;
					}
					out += "\\e[";
					out += content;
					i = j;
					continue;
				} else if(next == ']'){
					size_t j = i + 2;
					string content;
					bool terminator_found = false;
					while(j < n){
						unsigned char c = (unsigned char)s[j++];
						if(c == '\a'){ terminator_found = true; break; }
						if(c == 0x1B && j < n && s[j] == '\\'){ terminator_found = true; ++j; break; }
						content.push_back((char)c);
					}
					out += "\\e]";
					out += content;
					if(terminator_found) out += "<TERM>";
					i = j;
					continue;
				} else {
					out += "\\e";
					out.push_back((char)next);
					i += 2;
					continue;
				}
			} else {
				out += "\\e";
				++i;
				continue;
			}
		} else {
			out.push_back((char)ch);
			++i;
		}
	}
	return out;
}

char pick_neighbor(char ch){
	char lower = (char)tolower((unsigned char)ch);
	string key; key.push_back(lower);
	auto it = neigh.find(key);
	if(it != neigh.end()){
		const string &nset = it->second;
		uniform_int_distribution<int> d(0, (int)nset.size()-1);
		char w = nset[d(rng)];
		if(isupper((unsigned char)ch)) w = (char)toupper((unsigned char)w);
		return w;
	}
	return ch;
}

bool is_mistake(){
	if(MISTAKE_CHANCE <= 0) return false;
	uniform_int_distribution<int> d(1,100);
	int r = d(rng);
	if(r <= MISTAKE_CHANCE) return true;
	this_thread::sleep_for(chrono::duration<double>(calc_delay()));
	return false;
}

void print_hide_cursor(){ cout << "\x1B[?25l" << flush; }
void print_show_cursor(){ cout << "\x1B[?25h" << flush; }

void print_help(const string &prog_base){
	cout << prog_base << " v1.0 (c) Kamil BuriXon Burek 2026\n\n";
	cout << "Usage:\n";
	cout << "  " << prog_base << " [options] [file]\n\n";
	cout << "Options:\n";
	cout << "  -s, --speed <1-100>		 Typing speed (default 50). 100 = minimal delay.\n";
	cout << "  -m, --mistakes <1-100	 Enable random mistakes. Optionally set chance 1-100 (default off|10).\n";
	cout << "  -c, --color				 Interpret ANSI escape sequences (emit colors).\n";
	cout << "  -e, --print-escapes		 Print ANSI escapes textually as \\e[..., not as colors.\n";
	cout << "							 (conflicts with -c/--color)\n";
	cout << "  -b, --beep				 Emit BEL on non-zero exit code.\n";
	cout << "  -t, --text <string>		 Add a text line to display (can be repeated).\n";
	cout << "  -a, --show-all			 Force showing input even if detected as binary.\n";
	cout << "  -n, --line-numbers		 Prepend dimmed line numbers (N| ) to each line.\n";
	cout << "  -r, --allow-resize		 Allow terminal resize (SIGWINCH) during typing.\n";
	cout << "  -h, --help				 Show this help and exit.\n";
	cout << "  -v, --version			 Show program version and exit.\n";
	cout << "  --codes					 Show a list of exit codes and signal handling details.\n\n";
	cout << "Input:\n";
	cout << "  If no file is provided and stdin is a TTY, program reads lines as you\n";
	cout << "  type them (press Enter to send a line). If stdin is piped, the whole\n";
	cout << "  input is consumed and displayed.\n\n";
	cout << "License: GPLv3.0\n";
}

void print_version(const string &prog_base){
	cout << prog_base << " v1.0 (c) Kamil BuriXon Burek 2026\n";
}

void type_line(const string &raw_in, int lineno = -1, int total_lines = 0);

// helper: emit BEL if enabled
inline void maybe_bell(){
	if(beep_on_error){
		cout << '\a' << flush;
	}
}

void print_error_and_exit(int code, const string &msg){
	string prefix = string("\x1B[31m") + "error (" + to_string(code) + "):" + "\x1B[0m";
	string formatted = prefix + " " + msg;

	bool old_esc = escapes;
	bool old_print_esc = print_escapes;
	escapes = true;
	print_escapes = false;

	maybe_bell();
	print_hide_cursor();
	type_line(formatted, -1, 0);
	maybe_bell();

	escapes = old_esc;
	print_escapes = old_print_esc;

	exit(code);
}

void print_codes_and_exit(){
	cout << "Exit codes and signals handled by typecat:\n\n";
	cout << "Standard exit codes:\n";
	cout << "  0   - OK\n";
	cout << "  1   - Output is not a TTY (cannot pipe/redirect)\n";
	cout << "  2   - Invalid speed parameter (use 1-100)\n";
	cout << "  3   - Invalid mistakes parameter (use 1-100)\n";
	cout << "  4   - Input appears to be binary (stdin). Use -a/--show-all to override.\n";
	cout << "  5   - File cannot be read (permission denied / cannot open)\n";
	cout << "  6   - Unknown option / bad parameter / option conflict\n";
	cout << "  7   - Other runtime error\n";
	cout << "  8   - File does not exist\n";
	cout << "  9   - File is empty\n";
	cout << " 10   - File appears to be binary (file). Use -a/--show-all to override.\n\n";
	cout << "Signals (program exits with 128 + signal number unless allow-resize is enabled for SIGWINCH):\n";
	cout << "  SIGINT	(2)  -> exit 130   - Interrupted by user (Ctrl-C)\n";
	cout << "  SIGTERM	(15) -> exit 143   - Termination request\n";
	cout << "  SIGQUIT	(3)  -> exit 131   - Quit from keyboard\n";
	cout << "  SIGHUP	(1)  -> exit 129   - Hangup detected on controlling terminal\n";
#ifdef SIGWINCH
	cout << "  SIGWINCH (" << SIGWINCH << ") -> exit " << (128 + SIGWINCH)
		 << "	- Window size change; by default the program will print a signal line and an error indicating that resizing during typing is not advised, then exit.\n"
		 << "		Use -r/--allow-resize to ignore resize events.\n";
#else
	cout << "  SIGWINCH -> window-size change (handled if available on platform)\n";
#endif
	exit(0);
}

void handle_signal_event(int signo){
	string sig_macro;
	switch(signo){
		case SIGINT: sig_macro = "SIGINT"; break;
		case SIGTERM: sig_macro = "SIGTERM"; break;
		case SIGQUIT: sig_macro = "SIGQUIT"; break;
		case SIGHUP: sig_macro = "SIGHUP"; break;
#ifdef SIGWINCH
		case SIGWINCH: sig_macro = "SIGWINCH"; break;
#endif
		default:
			sig_macro = string("SIG") + to_string(signo);
	}
	int exit_code = 128 + signo;
	const char *desc = strsignal(signo);

#ifdef SIGWINCH
	if(signo == SIGWINCH){
		if(allow_resize){
			return;
		}
		maybe_bell();
		print_hide_cursor();
		cout << '\n';
		string sig_prefix = string("\x1B[33m") + "signal " + sig_macro + " (" + to_string(exit_code) + "):" + "\x1B[0m";
		cout << sig_prefix << " " << (desc ? desc : "") << endl;
		maybe_bell();

		string err_prefix = string("\x1B[31m") + "error (" + to_string(exit_code) + "):" + "\x1B[0m";
		string msg = "Resizing during typing is not advised and may corrupt output. "
					 "Note: some terminal environments can send SIGWINCH when focus changes. Use -r/--allow-resize to ignore resize events.";
		cout << err_prefix << " " << msg << endl;
		maybe_bell();

		cout.flush();
		exit(exit_code);
	}
#endif

	maybe_bell();
	print_hide_cursor();
	cout << '\n';
	string sig_prefix = string("\x1B[33m") + "signal " + sig_macro + " (" + to_string(exit_code) + "):" + "\x1B[0m";
	cout << sig_prefix << " " << (desc ? desc : "") << endl;
	maybe_bell();

	exit(exit_code);
}

void signal_handler(int signo){
	sig_flag = signo;
	if(sig_pipe_fds[1] != -1){
		uint8_t b = 1;
		ssize_t r = write(sig_pipe_fds[1], &b, 1);
		(void)r;
	}
}

void drain_sig_pipe(){
	if(sig_pipe_fds[0] == -1) return;
	uint8_t buf[128];
	while(true){
		ssize_t r = read(sig_pipe_fds[0], buf, sizeof(buf));
		if(r <= 0) break;
	}
}

void install_signal_handlers(){
	if(pipe(sig_pipe_fds) != 0){
		// if pipe fails, still attempt to set handlers without self-pipe
		sig_pipe_fds[0] = sig_pipe_fds[1] = -1;
	} else {
		int flags = fcntl(sig_pipe_fds[0], F_GETFL, 0);
		fcntl(sig_pipe_fds[0], F_SETFL, flags | O_NONBLOCK);
		flags = fcntl(sig_pipe_fds[1], F_GETFL, 0);
		fcntl(sig_pipe_fds[1], F_SETFL, flags | O_NONBLOCK);
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0; // do not SA_RESTART: allow syscalls to be interrupted
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGQUIT, &sa, nullptr);
	sigaction(SIGHUP, &sa, nullptr);
#ifdef SIGWINCH
	sigaction(SIGWINCH, &sa, nullptr);
#endif
}

int digits_count(int x){
	if(x <= 0) return 1;
	return (int)floor(log10((double)x)) + 1;
}

void sanitize_trailing_esc(string &s){
	size_t n = s.size();
	if(n == 0) return;
	size_t pos = s.find_last_of('\x1B');
	if(pos == string::npos) return;
	size_t i = pos + 1;
	if(i >= n){
		s.erase(pos);
		return;
	}
	unsigned char next = (unsigned char)s[i];
	if(next == '['){
		bool terminator_found = false;
		for(size_t k = i+1; k < n; ++k){
			unsigned char cc = (unsigned char)s[k];
			if(cc >= 0x40 && cc <= 0x7E){ terminator_found = true; break; }
		}
		if(!terminator_found){ s.erase(pos); return; }
	} else if(next == ']'){
		bool terminator_found = false;
		for(size_t k = i+1; k < n; ++k){
			if(s[k] == '\a'){ terminator_found = true; break; }
			if(s[k] == 0x1B && (k+1) < n && s[k+1] == '\\'){ terminator_found = true; break; }
		}
		if(!terminator_found){ s.erase(pos); return; }
	} else {
		return;
	}
}

void type_line(const string &raw_in, int lineno, int total_lines){
	string raw = raw_in;
	string line;

	if(print_escapes){
		string tmp = raw;
		replace_all(tmp, "\\e", string(1, '\x1B'));
		replace_all(tmp, "\\x1b", string(1, '\x1B'));
		replace_all(tmp, "\\033", string(1, '\x1B'));
		line = render_escapes_as_text(tmp);
	} else if(escapes){
		line = raw;
		replace_all(line, "\\e", string(1, '\x1B'));
		replace_all(line, "\\x1b", string(1, '\x1B'));
		replace_all(line, "\\033", string(1, '\x1B'));
	} else {
		line = strip_ansi(raw);
		replace_all(line, "\\e", "");
		replace_all(line, "\\x1b", "");
		replace_all(line, "\\033", "");
	}

	if(debug_enabled){
		string dbg_prefix = string("\x1B[36m") + "DEBUG:" + "\x1B[0m";
		cerr << dbg_prefix << " typing line";
		if(lineno >= 1) cerr << " " << lineno << "/" << (total_lines > 0 ? total_lines : lineno);
		cerr << " cols=" << get_cols() << " speed=" << speed << " allow-resize=" << (allow_resize ? "ON" : "OFF")
			 << " binary=" << (input_is_binary ? "YES" : "NO") << endl;
	}

	string prefix_full_str;
	string prefix_cont_str;
	int prefix_visible_len = 0;
	if(line_numbers && lineno >= 1){
		int width = digits_count(total_lines > 0 ? total_lines : max(1, lineno));
		string visible;
		if(input_is_binary){
			string q(width - 1, ' ');
			q += '?';
			visible = q + "| ";
		} else {
			string num = to_string(lineno);
			if((int)num.size() < width) num = string(width - (int)num.size(), ' ') + num;
			visible = num + "| ";
		}
		prefix_visible_len = (int)visible.size();
		prefix_full_str = string("\x1B[2m") + visible + "\x1B[0m";
		prefix_cont_str = string("\x1B[2m") + string(width, ' ') + "| " + "\x1B[0m";
	}

	string out;
	int j = 0;

	if(prefix_visible_len > 0){
		out = prefix_full_str;
		j = prefix_visible_len;
	} else {
		out.clear();
		j = 0;
	}

	int len = (int)line.size();
	int i = 0;
	while(i < len){
		if(sig_flag){
			int signo = sig_flag;
			sig_flag = 0;
			handle_signal_event(signo);
		}

		char ch = line[i];
		int cols = get_cols();

		if(escapes && ch == '\x1B'){
			string esc;
			esc.push_back(ch);
			++i;
			if(i >= len){ out += esc; cout << '\r' << "\x1B[K" << out << "█" << flush; break; }
			char next = line[i++];
			esc.push_back(next);
			if(next == '['){
				while(i < len){
					char c = line[i++];
					esc.push_back(c);
					if((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E) break;
				}
				out += esc; cout << '\r' << "\x1B[K" << out << "█" << flush; continue;
			}
			if(next == ']'){
				while(i < len){
					char c = line[i++];
					esc.push_back(c);
					if(c == '\a') break;
					if(c == '\x1B' && i < len && line[i] == '\\'){ esc.push_back('\\'); ++i; break; }
				}
				out += esc; cout << '\r' << "\x1B[K" << out << "█" << flush; continue;
			}
			out += esc; cout << '\r' << "\x1B[K" << out << "█" << flush; continue;
		}

		int delta = 1;
		if(ch == '\t'){
			int mod = j % TABSIZE;
			delta = TABSIZE - mod;
			if(delta == 0) delta = TABSIZE;
			cout << '\r' << "\x1B[K" << out << "█" << flush;
			for(int X=0; X<6; ++X){
				if(sig_flag){ int signo = sig_flag; sig_flag = 0; handle_signal_event(signo); }
				this_thread::sleep_for(chrono::duration<double>(calc_delay()));
			}
		} else {
			cout << '\r' << "\x1B[K" << out << "█" << flush;
			for(int X=0; X<3; ++X){
				if(sig_flag){ int signo = sig_flag; sig_flag = 0; handle_signal_event(signo); }
				this_thread::sleep_for(chrono::duration<double>(calc_delay()));
			}
			delta = 1;
		}

		int prospective = j + delta;
		if(prospective >= cols){
			cout << '\r' << "\x1B[K" << out << '\n' << flush;
			if(prefix_visible_len > 0){
				out = prefix_cont_str;
				cout << out << "█" << flush;
				j = prefix_visible_len;
			} else {
				out.clear();
				cout << "█" << flush;
				j = 0;
			}
			for(int X=0; X<2; ++X){
				if(sig_flag){ int signo = sig_flag; sig_flag = 0; handle_signal_event(signo); }
				this_thread::sleep_for(chrono::duration<double>(calc_delay()));
			}
		} else {
			j = prospective;
		}

		if(mistakes && ch != '\t' && ch != '\n' && ch != ' ' && is_mistake()){
			char wrong = pick_neighbor(ch);
			cout << '\r' << "\x1B[K" << out << wrong << "█" << flush;
			for(int X=0; X<5; ++X){
				if(sig_flag){ int signo = sig_flag; sig_flag = 0; handle_signal_event(signo); }
				this_thread::sleep_for(chrono::duration<double>(calc_delay()));
			}
			cout << '\r' << "\x1B[K" << out << "█" << flush;
			for(int X=0; X<10; ++X){
				if(sig_flag){ int signo = sig_flag; sig_flag = 0; handle_signal_event(signo); }
				this_thread::sleep_for(chrono::duration<double>(calc_delay()));
			}
		}

		out.push_back(ch);
		cout << '\r' << "\x1B[K" << out << "█" << flush;
		++i;
	}

	sanitize_trailing_esc(out);
	cout << '\r' << "\x1B[K" << out << '\n' << flush;

	cout << "█" << flush;
	for(int X=0; X<6; ++X){
		if(sig_flag){ int signo = sig_flag; sig_flag = 0; handle_signal_event(signo); }
		this_thread::sleep_for(chrono::duration<double>(calc_delay()));
	}
	cout << '\r' << "\x1B[K" << flush;

	if(debug_enabled){
		string dbg_prefix = string("\x1B[36m") + "DEBUG:" + "\x1B[0m";
		cerr << dbg_prefix << " finished line";
		if(lineno >= 1) cerr << " " << lineno;
		cerr << " cols=" << get_cols() << " speed=" << speed << endl;
	}
}

int main(int argc, char **argv){
	install_signal_handlers();
	atexit([](){
		print_show_cursor();
		if(sig_pipe_fds[0] != -1) close(sig_pipe_fds[0]);
		if(sig_pipe_fds[1] != -1) close(sig_pipe_fds[1]);
	});

	if(!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO)){
		print_error_and_exit(1, "Output cannot be piped or redirected. (FD: 1/2)");
	}

	vector<string> args;
	for(int i=1;i<argc;++i) args.push_back(string(argv[i]));

	for(size_t idx=0; idx<args.size(); ++idx){
		string a = args[idx];
		if(a=="-s" || a=="--speed"){
			if(idx+1<args.size()){
				string v = args[++idx];
				bool ok=true;
				for(char c: v) if(!isdigit((unsigned char)c)){ ok=false; break; }
				if(ok){
					int val = stoi(v);
					if(val>0 && val<=100){ speed = val; }
					else { print_error_and_exit(2, string("Invalid speed parameter: ") + v); }
				} else { print_error_and_exit(2, string("Invalid speed parameter: ") + v); }
			} else { print_error_and_exit(2, "Missing speed parameter"); }
		} else if(a=="-m" || a=="--mistakes"){
			mistakes = true;
			if(idx+1<args.size()){
				string v = args[idx+1];
				if(!v.empty() && isdigit((unsigned char)v[0])){
					idx++;
					int val = stoi(v);
					if(val>0 && val<=100){ MISTAKE_CHANCE = val; }
					else { print_error_and_exit(3, string("Invalid mistakes parameter: ") + v); }
				}
			}
		} else if(a=="-c" || a=="--color"){
			escapes = true;
		} else if(a=="-e" || a=="--print-escapes"){
			print_escapes = true;
		} else if(a=="-b" || a=="--beep"){
			beep_on_error = true;
		} else if(a=="-t" || a=="--text"){
			if(idx+1<args.size()){ texts.push_back(args[++idx]); } else { texts.push_back(string()); }
		} else if(a=="-a" || a=="--show-all"){
			show_all = true;
		} else if(a=="-n" || a=="--line-numbers"){
			line_numbers = true;
		} else if(a=="-r" || a=="--allow-resize"){
			allow_resize = true;
		} else if(a=="--debug"){
			debug_enabled = true;
		} else if(a=="-h" || a=="--help"){
			string prog_base = basename_of(argv[0]);
			print_help(prog_base);
			return 0;
		} else if(a=="-v" || a=="--version"){
			string prog_base = basename_of(argv[0]);
			print_version(prog_base);
			return 0;
		} else if(a=="--codes"){
			print_codes_and_exit();
		} else {
			if(a.size() > 0 && a[0] == '-'){
				print_error_and_exit(6, string("Unknown option: ") + a);
			}
			if(file_input.empty()) file_input = a;
		}
	}

	if(escapes && print_escapes){
		print_error_and_exit(6, "Options -c/--color and -e/--print-escapes are mutually exclusive");
	}

	if(!isatty(STDIN_FILENO) && texts.empty() && file_input.empty()){
		stdin_mode = true;
		ostringstream buf;
		buf << cin.rdbuf();
		string raw_in = buf.str();
		if(looks_binary(raw_in) && !show_all){
			print_error_and_exit(4, "Input appears to be binary. Use -a/--show-all to force display.");
		}
		input_is_binary = looks_binary(raw_in);
		istringstream iss(raw_in);
		string line;
		while(getline(iss, line)) texts.push_back(line);
	}

	if(!file_input.empty()){
		if(access(file_input.c_str(), F_OK) != 0){
			print_error_and_exit(8, string("File does not exist: ") + file_input);
		}
		if(access(file_input.c_str(), R_OK) != 0){
			print_error_and_exit(5, string("Cannot read file (permission denied): ") + file_input);
		}

		ifstream f(file_input, ios::in | ios::binary);
		if(!f){
			print_error_and_exit(5, string("Cannot open file for reading: ") + file_input);
		}
		ostringstream buf;
		buf << f.rdbuf();
		string raw_in = buf.str();

		if(raw_in.empty()){
			print_error_and_exit(9, string("File is empty: ") + file_input);
		}

		if(looks_binary(raw_in) && !show_all){
			print_error_and_exit(10, "File appears to be binary. Use -a/--show-all to force display.");
		}
		input_is_binary = looks_binary(raw_in);
		istringstream iss(raw_in);
		string line;
		while(getline(iss, line)) texts.push_back(line);
	}

	if(!texts.empty()){
		int total_lines = (int)texts.size();
		print_hide_cursor();
		for(int idx = 0; idx < total_lines; ++idx){
			if(sig_flag){
				int signo = sig_flag;
				sig_flag = 0;
				handle_signal_event(signo);
			}
			if(line_numbers){
				type_line(texts[idx], idx+1, total_lines);
			} else {
				type_line(texts[idx], -1, 0);
			}
		}
		if(debug_enabled){
			string success_prefix = string("\x1B[32m") + "success (0):" + "\x1B[0m";
			cout << success_prefix << " " << "work finished successfully! (allow-resize: " << (allow_resize ? "ENABLED" : "DISABLED") << ")" << endl;
		}
		return 0;
	}

	if(isatty(STDIN_FILENO) && file_input.empty()){
		string partial;
		int lineno = 0;
		const int BUF_SIZE = 4096;
		vector<char> buf(BUF_SIZE);

		while(true){
			if(sig_flag){
				int signo = sig_flag;
				sig_flag = 0;
				handle_signal_event(signo);
			}

			print_show_cursor();

			struct pollfd fds[2];
			fds[0].fd = STDIN_FILENO;
			fds[0].events = POLLIN;
			fds[0].revents = 0;

			fds[1].fd = (sig_pipe_fds[0] != -1) ? sig_pipe_fds[0] : -1;
			fds[1].events = (fds[1].fd != -1) ? POLLIN : 0;
			fds[1].revents = 0;

			int nfds = (fds[1].fd != -1) ? 2 : 1;
			int pres = poll(fds, nfds, -1);
			if(pres < 0){
				if(errno == EINTR) continue;
				print_error_and_exit(7, string("poll() failed: ") + strerror(errno));
			}

			if(nfds == 2 && (fds[1].revents & POLLIN)){
				drain_sig_pipe();
				if(sig_flag){
					int signo = sig_flag;
					sig_flag = 0;
					handle_signal_event(signo);
				}
			}

			if(fds[0].revents & (POLLIN|POLLERR|POLLHUP)){
				ssize_t r = read(STDIN_FILENO, buf.data(), BUF_SIZE);
				if(r < 0){
					if(errno == EINTR) continue;
					print_error_and_exit(7, string("read() failed: ") + strerror(errno));
				}
				if(r == 0){
					break;
				}
				partial.append(buf.data(), buf.data() + r);
				size_t pos;
				while((pos = partial.find('\n')) != string::npos){
					string line = partial.substr(0, pos);
					// remove optional '\r'
					if(!line.empty() && line.back() == '\r') line.pop_back();
					partial.erase(0, pos + 1);

					print_hide_cursor();
					++lineno;
					if(line_numbers){
						type_line(line, lineno, lineno);
					} else {
						type_line(line, -1, 0);
					}
				}
			}
		}
		if(debug_enabled){
			string success_prefix = string("\x1B[32m") + "success (0):" + "\x1B[0m";
			cout << success_prefix << " " << "work finished successfully! (allow-resize: " << (allow_resize ? "ENABLED" : "DISABLED") << ")" << endl;
		}
	}
	return 0;
}
