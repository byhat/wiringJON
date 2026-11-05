#!/bin/bash
# deploy_and_test.sh — sync the project to the Jetson, build, run tests.
# Usage: scripts/deploy_and_test.sh [--no-test]
set -e
cd "$(dirname "$0")/.."
J="scripts/jssh.py"

echo "=== sync to board ==="
python3 "$J" --sync . wiringJON .git third_party *.o *.so* a.out

echo "=== build on board ==="
python3 "$J" 'cd ~/wiringJON && make'

if [ "${1:-}" != "--no-test" ]; then
  echo "=== tests on board (wire: header pin 32 <-> pin 7) ==="
  python3 "$J" 'cd ~/wiringJON && tests/test_gpio phys && tests/test_gpio gpio && tests/test_gpio pins'
fi
