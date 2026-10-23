#!/bin/sh
# Tests tools/find-mix-site.ts on a synthetic OS file with planted references. Needs Node 22.18+ or 24.
set -e
cd "$(dirname "$0")/../.."
tmp="$(mktemp -d)"
node tests/tool/make-fake-os.ts "$tmp/fake.syx"
node tools/find-mix-site.ts "$tmp/fake.syx" --out "$tmp/report.json" | tee "$tmp/out.txt"
grep -q "0x8000ea70    2 refs" "$tmp/out.txt"
grep -q "0x80001234    1 refs" "$tmp/out.txt"
! grep -q "0x10000000" "$tmp/out.txt"
grep -q '"callTargetsNext48Bytes": \[' "$tmp/report.json"
grep -q "0x40012345" "$tmp/report.json"
echo "tool test: ok"
rm -rf "$tmp"
