#!/usr/bin/env bash
set -euo pipefail

CC=${CC:-gcc}
for tool in "$CC" file readelf objdump; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
done

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_file="$script_dir/day-007-startup-probe.c"
work=$(mktemp -d -t week3-startup-XXXXXX)
trap 'rm -rf "$work"' EXIT
binary="$work/startup-probe"

"$CC" -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
    "$source_file" -o "$binary"

echo "== ELF identity and entry =="
file "$binary"
readelf -h "$binary" | awk '/Type:|Machine:|Entry point/'

echo
echo "== Program headers and section-to-segment mapping =="
readelf -lW "$binary"

echo
echo "== Startup-related symbols and initialization arrays =="
readelf -sW "$binary" |
    awk '$8 == "_start" || $8 == "main" || $8 == "before_main" ||
         $8 == "normal_exit_handler"'
readelf -x .init_array "$binary"
objdump -d --disassemble=_start "$binary"

echo
echo "== Runtime probe =="
set +e
"$binary" alpha "two words" >"$work/runtime.log" 2>&1
status=$?
set -e
cat "$work/runtime.log"
printf 'observed exit status: %d\n' "$status"
test "$status" -eq 23
awk '
    /\[constructor\]/ { constructor = NR }
    /\[main\] constructor_ran=/ { main = NR }
    /\[atexit\]/ { handler = NR }
    END {
        if (!(constructor && main && handler &&
              constructor < main && main < handler))
            exit 1
    }
' "$work/runtime.log"

interp=$(readelf -lW "$binary" | awk '/Requesting program interpreter/ {
    sub(/^.*: /, ""); sub(/\]$/, ""); print
}')
printf '\nPT_INTERP requests: %s\n' "$interp"
if [[ -n "$interp" && -e "$interp" ]]; then
    printf 'interpreter exists and is: '
    file "$interp"
fi

echo
echo "== Dynamic-loader trace (captured, first 35 lines) =="
LD_DEBUG=libs,reloc "$binary" trace-only >"$work/stdout.log" 2>"$work/loader.log" || {
    status=$?
    test "$status" -eq 23
}
awk 'NR <= 35 { print }' "$work/loader.log"

echo
echo "The temporary executable and loader log will now be removed."
