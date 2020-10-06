#!/bin/bash
CC=gcc

assert() {
    expected="$1"
    input="$2"

    ./charmcc "$input" > tmp.s || exit
    $CC -o tmp tmp.s || exit
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $actual, expected $expected"
        exit 1
    fi
}

assert 0 0
assert 42 42
assert 21 '5+20-4'
assert 41 ' 12 + 34  -  5 '

echo OK
