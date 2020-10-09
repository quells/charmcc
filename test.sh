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

assert 0 '0;'
assert 42 '42;'
assert 21 '5+20-4;'
assert 41 ' 12 + 34  -  5 ;'
assert 47 '5 + 6*7;'
assert 77 '(5+6) * 7;'
assert 15 '5 * (9-6);'
assert 39 '5*9 - 6;'
assert 4  '(3+5) / 2;'
assert 5  '3 + 5/2;'
assert 0  '1 / 2;'
assert 1  '1024/16/8/4/2;'
assert 10 '-10 + 20;'
assert 10 '--10;'
assert 10 '--+10;'

assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'

assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'

assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'

echo OK
