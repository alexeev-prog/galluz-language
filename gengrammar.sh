#!/usr/bin/env bash

syntax-cli -g source/parser/galluzLangGrammar.bnf -m LALR1 -o source/parser/galluzLangGrammar.h
