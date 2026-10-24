#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
test_build_dir="$(mktemp -d)"
trap 'rm -rf -- "$test_build_dir"' EXIT

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I "$test_dir/stubs" "$test_dir/firmware_test.cpp" \
  -o "$test_build_dir/firmware-tests"
"$test_build_dir/firmware-tests"
