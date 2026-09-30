#!/usr/bin/env bash
set -euo pipefail

for tool in iverilog vvp verilator; do
    command -v "$tool" >/dev/null || {
        echo "missing tool: $tool" >&2
        exit 1
    }
done

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
rtl="$script_dir/day-006-alu-register-stage.sv"
tb="$script_dir/day-006-alu-register-stage-tb.sv"
work=$(mktemp -d -t day-006-rtl-XXXXXX)
trap 'rm -rf "$work"' EXIT

echo "== Verilator lint =="
verilator --lint-only --timing --Wall --Wno-DECLFILENAME \
    --top-module day_006_alu_register_stage_tb \
    "$rtl" "$tb"

echo
echo "== Icarus compile and simulation =="
iverilog -g2012 -Wall -Wimplicit -Wportbind -Wselect-range \
    -s day_006_alu_register_stage_tb \
    -o "$work/day-006-rtl.vvp" "$rtl" "$tb"

(
    cd "$script_dir"
    vvp "$work/day-006-rtl.vvp"
)

echo
echo "Waveform: $script_dir/day-006-alu-register-stage.vcd"
echo "Optional viewer: gtkwave '$script_dir/day-006-alu-register-stage.vcd'"
