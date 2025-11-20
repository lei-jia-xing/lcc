#!/bin/bash

set -e

cd build

./Compiler

cp mips.txt mips.s

java -jar ../MARS2025+.jar nc mips.s
