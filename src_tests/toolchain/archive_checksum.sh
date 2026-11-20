#!/usr/bin/env bash
set -euo pipefail

installer="$(dirname "$0")/../../.github/scripts/install-arm-toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/package/bin" "$work/valid" "$work/changed"
printf 'original\n' > "$work/package/bin/compiler"
tar -cJf "$work/compiler.tar.xz" -C "$work" package
checksum=$(sha256sum "$work/compiler.tar.xz" | cut -d' ' -f1)

export RUNNER_TEMP="$work/valid" GITHUB_PATH="$work/valid/path"
bash "$installer" "file://$work/compiler.tar.xz" "$checksum"
cmp "$work/package/bin/compiler" "$RUNNER_TEMP/compiler/bin/compiler"
test "$(cat "$GITHUB_PATH")" = "$RUNNER_TEMP/compiler/bin"

# A different, still extractable archive must fail before extraction and PATH publication.
printf 'different\n' > "$work/package/bin/compiler"
tar -cJf "$work/compiler.tar.xz" -C "$work" package
export RUNNER_TEMP="$work/changed" GITHUB_PATH="$work/changed/path"
if bash "$installer" "file://$work/compiler.tar.xz" "$checksum"; then
    echo "Installer accepted an archive with the wrong SHA-256" >&2
    exit 1
fi
test ! -e "$RUNNER_TEMP/compiler"
test ! -s "$GITHUB_PATH"
