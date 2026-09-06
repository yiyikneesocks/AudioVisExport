#!/bin/bash
# clang-cl wrapper: invokes clang-18 with --target to get MSVC ABI mode
# (clang-18 needed: MSVC CRT headers use C++23 static_assert in templates)
exec clang-18 --target=x86_64-w64-windows-msvc "$@"
