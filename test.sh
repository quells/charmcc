#!/bin/bash
assert() {
    expected="$1"
    input="$2"

    ./charmcc "$input" > tmp.s || exit
    as tmp.s -o tmp.o || exit
    gcc -o tmp tmp.o || exit
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

echo OK
