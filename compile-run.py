#!/usr/bin/env python3
import os
import sys


def build():
    os.system("bash build.sh all")


def run_galluzlang():
    os.system("./build/bin/galluzlang")


def clang_compile():
    os.system("clang++ -O3 ./out.ll -o ./out.bin")


def run():
    os.system("""./out.bin
echo $?""")


build()
run_galluzlang()

if len(sys.argv) > 1 and sys.argv[1] == "build":
    clang_compile()
    run()
