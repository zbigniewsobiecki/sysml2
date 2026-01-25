#!/bin/bash
cd /Users/zbigniew/Code/sysml2/grammar && \
packcc -o sysml_parser sysml.peg && \
cp sysml_parser.c sysml_parser.h ../src/ && \
cd ../build && \
ninja 2>&1 | tail -20
