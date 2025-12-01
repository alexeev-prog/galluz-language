#!/usr/bin/env bash

syntax-cli -g source/parser/GalluzGrammar.bnf -m LALR1 -o source/parser/GalluzGrammar.h
