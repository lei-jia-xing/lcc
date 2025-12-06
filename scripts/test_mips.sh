#!/bin/bash

set -e

cd build

./Compiler

java -jar ../MARS2025+.jar nc mips.txt
