#!/bin/sh
# Builds and runs the host tests for the parts that do not need the ESP32 (podcast feed parsing,
# file names, download planning). Uses the system C++ compiler.
set -e
cd "$(dirname "$0")/.."
mkdir -p .pio/host-test
c++ -std=c++17 -Wall -Wextra -O1 -o .pio/host-test/feed_test test/host/feed_test.cpp
.pio/host-test/feed_test test/host/fixtures
