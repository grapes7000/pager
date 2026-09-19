#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary=$(mktemp /tmp/pager-tests.XXXXXX)
trap 'rm -f "$test_binary"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
  -Itests/host -Iinclude tests/host/regression.cpp \
  src/composer.cpp src/encoder.cpp src/message_store.cpp src/message_view.cpp \
  -o "$test_binary"
"$test_binary"
echo 'Host regression tests passed.'
