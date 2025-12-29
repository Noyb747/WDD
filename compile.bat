@echo off
gcc wdd.c -o wdd.exe -std=c11 -O2 -Wall -Wextra -Wpedantic ^
    -Wno-unused-parameter -DWIN32_LEAN_AND_MEAN -municode -static-libgcc -static-libstdc++ -s

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build successful
