#!/usr/bin/env bash
set -euo pipefail

CC=${CC:-gcc}
for tool in "$CC" ar readelf nm objdump ldd; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
done

work=$(mktemp -d -t week3-link-XXXXXX)
trap 'rm -rf "$work"' EXIT
cd "$work"
printf 'workspace: %s\n' "$work"

cat > mathops.h <<'EOF'
#ifndef MATHOPS_H
#define MATHOPS_H
int twice(int value);
int library_generation(void);
#endif
EOF

cat > mathops.c <<'EOF'
#include "mathops.h"
int twice(int value) { return value * 2; }
int library_generation(void) { return 1; }
EOF

cat > main.c <<'EOF'
#include <stdio.h>
#include "mathops.h"
int main(void)
{
    printf("generation=%d result=%d\n", library_generation(), twice(21));
    return 0;
}
EOF

common=(-std=c17 -Wall -Wextra -Wpedantic -Werror -O2)

echo
echo "== Relocatable clients and providers =="
"$CC" "${common[@]}" -c main.c -o main.o
"$CC" "${common[@]}" -c mathops.c -o mathops.o
nm main.o
readelf -Wr main.o

echo
echo "== Static archive =="
ar rcs libmathops.a mathops.o
"$CC" main.o -L. -lmathops -o app-archive
./app-archive
ls -l app-archive libmathops.a

echo
echo "== Shared object, linked with a safe experiment-only RUNPATH =="
"$CC" "${common[@]}" -fPIC -c mathops.c -o mathops.pic.o
"$CC" -shared -Wl,-soname,libmathops.so.1 -o libmathops.so.1.0 mathops.pic.o
ln -s libmathops.so.1.0 libmathops.so.1
ln -s libmathops.so.1 libmathops.so
"$CC" main.o -L. -lmathops -Wl,-rpath,'$ORIGIN' -o app-shared
readelf -d app-shared | sed -n '/NEEDED\|RUNPATH/p'
ldd ./app-shared
./app-shared

echo
echo "== Dynamic-call machinery =="
objdump -d -j .plt ./app-shared 2>/dev/null || true
readelf -Wr ./app-shared

echo
echo "== Intentional undefined-symbol diagnosis =="
cat > missing.c <<'EOF'
extern int absent(void);
int main(void) { return absent(); }
EOF
if "$CC" "${common[@]}" missing.c -o broken 2>missing.log; then
    echo "expected undefined-symbol link to fail" >&2
    exit 1
fi
cat missing.log

echo
echo "All generated files were confined to $work and will now be removed."
