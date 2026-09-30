#!/usr/bin/env bash
# Observe one small RTL design before and after generic technology mapping.
set -euo pipefail

command -v yosys >/dev/null || {
    echo "missing required tool: yosys" >&2
    exit 1
}

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
src="$repo/04-cpu-and-chip-design/challenges/day-007-synthesis-demo.sv"
tb="$repo/04-cpu-and-chip-design/challenges/day-007-synthesis-testbench.sv"
out=${DAY007_BUILD_DIR:-/tmp/week4-day007-yosys}

rm -rf "$out"
mkdir -p "$out"

echo "Yosys observation workspace: $out"
yosys -V | tee "$out/tool-version.txt"

if command -v iverilog >/dev/null && command -v vvp >/dev/null; then
    echo
    echo "== Optional RTL simulation =="
    iverilog -g2012 -Wall -s synthesis_demo_tb \
        -o "$out/simulation.vvp" "$src" "$tb"
    vvp "$out/simulation.vvp" | tee "$out/simulation.txt"
else
    echo "optional iverilog/vvp not found; synthesis observation continues"
fi

cat >"$out/observe.ys" <<EOF
read_verilog -sv $src
hierarchy -check -top synthesis_demo

# Lower procedural RTL into generic internal operations and storage.
proc
opt
tee -o $out/generic-stat.txt stat
write_verilog -noattr $out/generic-netlist.v
write_json $out/generic-netlist.json
show -format dot -prefix $out/generic synthesis_demo

# Map words and generic operations into Yosys internal gate primitives.
techmap
opt
abc -g AND,OR,XOR,XNOR,MUX
clean
tee -o $out/mapped-stat.txt stat
write_verilog -noattr $out/mapped-netlist.v
write_json $out/mapped-netlist.json
show -format dot -prefix $out/mapped synthesis_demo
EOF

echo
echo "== One Yosys RTL-to-netlist observation =="
yosys -q -l "$out/yosys.log" "$out/observe.ys"

test -s "$out/generic-stat.txt"
test -s "$out/mapped-stat.txt"
test -s "$out/generic-netlist.v"
test -s "$out/mapped-netlist.v"
test -s "$out/generic.dot"
test -s "$out/mapped.dot"

echo
echo "== Generic inferred cells =="
rg '\$([a-zA-Z_]+)' "$out/generic-stat.txt" "$out/generic-netlist.v" |
    rg '\$(add|mux|dff|sdff|sdffe|dffe)' || true

echo
echo "== Mapped cell summary =="
rg '\$_[A-Z0-9_]+' "$out/mapped-stat.txt" "$out/mapped-netlist.v" |
    sort -u || true

if command -v dot >/dev/null; then
    dot -Tsvg "$out/generic.dot" -o "$out/generic.svg"
    dot -Tsvg "$out/mapped.dot" -o "$out/mapped.svg"
    echo "Graphviz SVGs: $out/generic.svg and $out/mapped.svg"
fi

echo
echo "Artifacts retained under $out"
echo "These generic mappings do not establish transistor-accurate area,"
echo "power, timing, placement, routing, or signoff."
