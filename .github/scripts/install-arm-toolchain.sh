#!/usr/bin/env bash
set -euo pipefail

url=$1
expected_sha256=$2
archive="$RUNNER_TEMP/${url##*/}"
toolchain_dir=${archive%.tar.xz}

curl --fail --location --silent --show-error --retry 3 --output "$archive" "$url"
printf '%s  %s\n' "$expected_sha256" "$archive" | sha256sum --check --strict
mkdir -p "$toolchain_dir"
tar -xf "$archive" -C "$toolchain_dir" --strip-components=1
printf '%s/bin\n' "$toolchain_dir" >> "$GITHUB_PATH"
