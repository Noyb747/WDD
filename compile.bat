@echo off
gcc wdd.c ^
  -o wdd.exe ^
  -std=c11 ^
  -O2 ^
  -Wall -Wextra -Wpedantic ^
  -DWIN32_LEAN_AND_MEAN ^
  -municode ^
  -static-libgcc ^
  -s

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build successful
