#!/bin/sh
black -q emulator
clang-format -i tests/*.c
