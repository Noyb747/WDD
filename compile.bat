@echo off
gcc wdd.c -o wdd.exe -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -municode -static-libgcc -static-libstdc++ -s
