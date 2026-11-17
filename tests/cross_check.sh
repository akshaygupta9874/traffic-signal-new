#!/bin/sh
# C++ engine vs JavaScript engine: identical traces for several configurations.
set -e
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O2 -o build/trace examples/trace.cpp core/traffic_config.cpp core/traffic_engine.cpp
fail=0
for args in "10 3 10 0.25" "5 2 7 0.5" "1 1 1 0.3" "20 4 15 0.1"; do
  ./build/trace $args > build/trace_cpp.txt
  node tests/trace.js $args > build/trace_js.txt
  if cmp -s build/trace_cpp.txt build/trace_js.txt; then echo "PASS  C++ == JS  (green yellow red dt = $args)  $(wc -l < build/trace_cpp.txt) lines";
  else echo "FAIL  traces differ for: $args"; diff build/trace_cpp.txt build/trace_js.txt | head -5; fail=1; fi
done
exit $fail
