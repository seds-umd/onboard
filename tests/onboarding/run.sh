#!/bin/sh
set -eu
cd "$(dirname "$0")"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
for name in sos onboard gyro; do
  extra_flags=""
  # Including the existing driver exposes one unused private helper.
  if [ "$name" = gyro ]; then extra_flags="-Wno-unused-function"; fi
  cc -std=c11 -Wall -Wextra -Werror $extra_flags -I stubs -I ../../include "test_$name.c" -lm -o "$build_dir/test_$name"
  "$build_dir/test_$name"
done
