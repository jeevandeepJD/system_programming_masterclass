#!/usr/bin/env bash
set -euo pipefail

for tool in verilator iverilog vvp; do
    command -v "$tool" >/dev/null || {
        echo "missing required tool: $tool" >&2
        exit 1
    }
done

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
design="$script_dir/day-002-alu-register-file.sv"
testbench="$script_dir/day-002-alu-register-file-tb.sv"
work=$(mktemp -d -t day-002-rv32-blocks-XXXXXX)
trap 'rm -rf "$work"' EXIT

echo "== Verilator lint =="
verilator --lint-only --timing --Wall --Wno-DECLFILENAME \
    --top-module day_002_alu_register_file_tb \
    "$design" "$testbench"

echo
echo "== Icarus compile and simulation =="
iverilog -g2012 -Wall -Wimplicit -Wportbind -Wselect-range \
    -s day_002_alu_register_file_tb \
    -o "$work/day-002-alu-register-file.vvp" \
    "$design" "$testbench"
(
    cd "$work"
    vvp "$work/day-002-alu-register-file.vvp"
)
