# typecat — A smart, lifelike text typer for your terminal ✍️

## About
**typecat** is a fast, lightweight and highly configurable command-line tool written in **C++ (C++17)** that displays text as if it were being typed in real time. It reads from files, stdin (pipes) or direct text arguments and renders output with realistic timing, optional typos, ANSI escape handling and line numbering.

It’s a fun and useful alternative to classic `cat` and modern `bat`/`batcat` — great for demos, screencasts, teaching, script testing or just spicing up your terminal output. Portable and easy to build on Linux, Termux and other Unix-like systems.

![Banner](/banner.gif)

## Features
- Realistic character-by-character typing simulation with configurable speed.
- Optional random typos with adjustable probability.
- Proper handling of ANSI escape sequences (render colors) or textual representation of escapes.
- Line numbers (dimmed) and continued line prefix support.
- Read from file, stdin (pipe) or `-t/--text` inline arguments.
- Binary input detection with `--show-all` override.
- Audible bell on errors (optional).
- Robust, deterministic POSIX signal handling (SIGINT, SIGTERM, SIGQUIT, SIGHUP, SIGWINCH).
- Debug mode for runtime diagnostics.
- Minimal external dependencies — just a C++17 toolchain.

## Installation

## Installation

### Install dependencies / Setup environment

**Debian / Ubuntu**
```
sudo apt update
sudo apt install -y build-essential g++ git
```

**Arch / Manjaro**
```
sudo pacman -Syu --needed base-devel gcc git
```

**Fedora / RHEL / CentOS**
```
sudo dnf install -y gcc-c++ make git
```
```
# for older RHEL/CentOS: 
sudo yum install -y gcc-c++ make git
```

**Alpine Linux**
```
sudo apk add build-base git
```

**Termux (Android)**
```
pkg update && pkg upgrade
pkg install git clang make
```

> The above installs the necessary C++ build tools and Git to fetch the source.

### Clone repository and build
```

git clone https://github.com/BuriXon-code/typecat
cd typecat
```

**Build with g++**
```
g++ -std=c++17 -O2 typecat.cpp -o typecat
chmod +x typecat
```

## Usage

> NOTE: typecat expects a TTY on stdout/stderr for full behavior. Using it with redirected output may fail unless reading from stdin (piped input) is intended.

### Options (summary)

| Option | Description |
|--------|-------------|
| `-s, --speed <1-100>` | Typing speed (default 50). Higher = faster / shorter delays. |
| `-m, --mistakes [1-100]` | Enable random typos. Optionally give chance as percent (default 10). |
| `-c, --color` | Interpret ANSI escape sequences (show colors). |
| `-e, --print-escapes` | Print ANSI escapes textually as `\e[...]` instead of interpreting them. |
| `-b, --beep` | Emit BEL on error conditions. |
| `-t, --text <string>` | Add a text line to display (multiple `-t` allowed). |
| `-a, --show-all` | Force showing input even if it looks binary. |
| `-n, --line-numbers` | Prepend dimmed line numbers. |
| `-r, --allow-resize` | Ignore SIGWINCH (allow terminal resize while typing). |
| `--debug` | Print debug info to stderr (useful for troubleshooting). |
| `-h, --help` | Show help and exit. |
| `-v, --version` | Show version and exit. |
| `--codes` | Show list of exit codes and signal handling info. |

### Examples

Simple: type a file slowly (default settings)
```
./typecat file.txt
```
Type a file with colors interpreted:
```
./typecat -c file_with_ansi.txt
```
Faster typing with random mistakes (15% chance):
```
./typecat -m 15 -s 75 file.txt
```
Inline text lines (no file):
```
./typecat -t \"Hello, world!\" -t \"This is typed.\"
```
Pipe output from another program:
```
echo \"live output\" | ./typecat -s 60
```
Show line numbers and colored output:
```
./typecat -n -c file.txt
```
Force display of file that appears binary:
```
./typecat -a suspicious.bin
```

Debug mode:
```
./typecat --debug -c file.txt
```

## License

**typecat** is released under the **GPL v3.0 (GNU General Public License v3.0)**.  

This license ensures that the software remains free and open-source. You are free to use, modify, and distribute it, but there are obligations to follow.

### You **can**:
- Use the program for personal, educational, or commercial purposes.
- Modify the source code to suit your needs.
- Share your modified or unmodified version of the program.
- Include it in other GPL-compatible projects.

### You **cannot**:
- Remove the original copyright notice and license.
- Distribute the software under a proprietary license.
- Claim the program as entirely your own work.
- Impose additional restrictions beyond GPLv3 when redistributing.

> ![NOTE]
> In short, GPLv3 allows freedom to use and modify, but ensures that the same freedoms are preserved for all downstream users.

## Support
### Contact me:
For any issues, suggestions, or questions, reach out via:

- *Email:* support@burixon.dev
- *Contact form:* [Click here](https://burixon.dev/contact/)
- *Bug reports:* [Click here](https://burixon.dev/bugreport/#typecat)

### Support me:
If you find this script useful, consider supporting my work by making a donation:

[**Donations**](https://burixon.dev/donate/)

Your contributions help in developing new projects and improving existing tools!
