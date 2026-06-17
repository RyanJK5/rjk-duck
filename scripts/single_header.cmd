REM This script produces a single header file from the several headers in the src/ directory. It uses the CLI tool
REM Amalgamate: https://github.com/rindeal/Amalgamate, and assumes amalgamate.exe is in the system's PATH.
REM It uses Windows batch syntax, and therefore can currently only be run on Windows.

@echo off

cd ..
amalgamate -i src/ -w "*.hpp;*.inl" src\duck_view.hpp include\rjk\duck.hpp