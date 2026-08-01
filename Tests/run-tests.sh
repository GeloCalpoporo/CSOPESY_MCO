#!/usr/bin/env bash
# CSOPESY MCO2 - black-box test runner
#
# Builds the emulator, then drives it through the CLI exactly the way a grader
# would: piped commands in, terminal output out, assertions checked against
# what was actually printed. There is no unit-test framework and none is wanted;
# the graded quiz is black-box, so the tests are black-box too.
#
#   ./run-tests.sh              run every case
#   ./run-tests.sh 03 07        run only cases whose folder name starts with 03 / 07
#   VERBOSE=1 ./run-tests.sh 03 show the full transcript of each case
#
# Output of every run is kept in Tests/out/<case>/ so a failure can be read
# afterwards, and so the transcript can be pasted into the technical report.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC="$ROOT/CSOPESY_Emulator"
OUT="$HERE/out"
EXE="$OUT/CSOPESY_Emulator.exe"

# MSYS2 runtime DLLs, if this is the machine that has them. Harmless elsewhere.
MINGW="/s/Main Programs/Productivity/Coding/MSYS2/mingw64/bin"
[ -d "$MINGW" ] && export PATH="$MINGW:$PATH"

rm -rf "$OUT"
mkdir -p "$OUT"

# ---------------------------------------------------------------- build

printf 'Building ... '
if ! g++ -std=c++20 -Wall -Wextra -O1 -o "$EXE" "$SRC"/*.cpp 2> "$OUT/build.log"; then
	printf 'FAILED\n\n'
	cat "$OUT/build.log"
	exit 1
fi
if [ -s "$OUT/build.log" ]; then
	printf 'ok, but with warnings\n\n'
	cat "$OUT/build.log"
else
	printf 'ok (zero warnings under -Wall -Wextra)\n'
fi

# ---------------------------------------------------------------- helpers

# Feed a command file to the emulator. Lines starting with '#' are comments;
# '@sleep N' pauses so the scheduler thread can actually run before the next
# command is typed. Without that, piped input arrives faster than the CPU ticks.
feed() {
	while IFS= read -r line || [ -n "$line" ]; do
		case "$line" in
			'#'*)       continue ;;
			'@sleep '*) sleep "${line#@sleep }" ;;
			*)          printf '%s\n' "$line" ;;
		esac
	done < "$1"
	sleep 0.2
}

PASS=0
FAIL=0
FAILED_CASES=""

run_case() {
	local dir="$1"
	local name; name="$(basename "$dir")"
	local wd="$OUT/$name"
	mkdir -p "$wd"

	# Each case gets a clean working directory, so the backing store and the
	# report log it produces belong to that case alone.
	if [ -f "$dir/config.txt" ]; then
		cp "$dir/config.txt" "$wd/config.txt"
	else
		cp "$SRC/config.txt" "$wd/config.txt"
	fi

	feed "$dir/cmds.txt" | ( cd "$wd" && "$EXE" ) > "$wd/stdout.txt" 2>&1

	# Generated files are appended to the transcript so assertions can reach
	# them with the same '+' / '~' syntax as terminal output.
	local t="$wd/transcript.txt"
	cp "$wd/stdout.txt" "$t"
	for f in csopesy-log.txt csopesy-backing-store.txt; do
		if [ -f "$wd/$f" ]; then
			printf '\n===== FILE %s =====\n' "$f" >> "$t"
			cat "$wd/$f" >> "$t"
		fi
	done

	# ------------------------------------------------------------ assertions
	local errors=0
	local checks=0
	local report=""

	while IFS= read -r a || [ -n "$a" ]; do
		case "$a" in ''|'#'*) continue ;; esac
		checks=$((checks + 1))
		local kind="${a%% *}"
		local rest="${a#* }"
		local ok=1

		case "$kind" in
			'+')  grep -Fq -- "$rest" "$t" || ok=0 ;;
			'-')  grep -Fq -- "$rest" "$t" && ok=0 ;;
			'~')  grep -Eq -- "$rest" "$t" || ok=0 ;;
			'N')  local want="${rest%% *}"
			      local needle="${rest#* }"
			      local got; got="$(grep -Fc -- "$needle" "$t")"
			      [ "$got" = "$want" ] || { ok=0; rest="$rest  (found $got)"; } ;;
			*)    ok=0; rest="unknown assertion type: $a" ;;
		esac

		if [ "$ok" = 0 ]; then
			errors=$((errors + 1))
			report="$report      x $kind $rest"$'\n'
		fi
	done < "$dir/expect.txt"

	if [ "$errors" = 0 ]; then
		PASS=$((PASS + 1))
		printf '  PASS  %-28s %2d checks\n' "$name" "$checks"
	else
		FAIL=$((FAIL + 1))
		FAILED_CASES="$FAILED_CASES $name"
		printf '  FAIL  %-28s %2d checks, %d failed\n' "$name" "$checks" "$errors"
		printf '%s' "$report"
	fi

	if [ "${VERBOSE:-0}" = 1 ]; then
		printf '      --- transcript ---\n'
		sed 's/^/      /' "$t"
		printf '      ------------------\n'
	fi
}

# ---------------------------------------------------------------- run

printf '\n'
for dir in "$HERE"/cases/*/; do
	name="$(basename "$dir")"
	if [ "$#" -gt 0 ]; then
		match=0
		for want in "$@"; do
			case "$name" in "$want"*) match=1 ;; esac
		done
		[ "$match" = 1 ] || continue
	fi
	run_case "$dir"
done

printf '\n%d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" = 0 ] || { printf 'failed:%s\n' "$FAILED_CASES"; exit 1; }
printf 'Transcripts in Tests/out/<case>/transcript.txt\n'
