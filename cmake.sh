#!/bin/bash

set -euo pipefail

cmake -S . -B build

./compile_flags.sh
