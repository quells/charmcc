#!/bin/bash
CC=gcc

cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add4(int a, int b, int c, int d) { return a+b+c+d; }
EOF

assert() {
    expected="$1"
    input="$2"

    ./charmcc "$input" > tmp.s || exit
    $CC -o tmp tmp.s tmp2.o || exit
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $actual, expected $expected"
        exit 1
    fi

    ./charmcc --debug "$input" > /dev/null || exit

    if [ -n "$VALGRIND" ]; then
        valgrind ./charmcc "$input" 2>&1 >/dev/null | grep 'no leaks are possible' >/dev/null
        leaky="$?"
        if [ "$leaky" = "1" ]; then
            echo "leak detected"
            exit 1
        fi
    fi
}

assert 0  'int main() { return 0; }'
assert 42 'int main() { return 42; }'
assert 21 'int main() { return 5+20-4; }'
assert 41 'int main() { return  12 + 34  -  5 ; }'
assert 47 'int main() { return 5 + 6*7; }'
assert 77 'int main() { return (5+6) * 7; }'
assert 15 'int main() { return 5 * (9-6); }'
assert 39 'int main() { return 5*9 - 6; }'
assert 4  'int main() { return (3+5) / 2; }'
assert 5  'int main() { return 3 + 5/2; }'
assert 0  'int main() { return 1 / 2; }'
assert 1  'int main() { return 1024/16/8/4/2; }'
assert 10 'int main() { return -10 + 20; }'
assert 10 'int main() { return --10; }'
assert 10 'int main() { return --+10; }'

assert 0 'int main() { return 0==1; }'
assert 1 'int main() { return 42==42; }'
assert 1 'int main() { return 0!=1; }'
assert 0 'int main() { return 42!=42; }'

assert 1 'int main() { return 0<1; }'
assert 0 'int main() { return 1<1; }'
assert 0 'int main() { return 2<1; }'
assert 1 'int main() { return 0<=1; }'
assert 1 'int main() { return 1<=1; }'
assert 0 'int main() { return 2<=1; }'

assert 1 'int main() { return 1>0; }'
assert 0 'int main() { return 1>1; }'
assert 0 'int main() { return 1>2; }'
assert 1 'int main() { return 1>=0; }'
assert 1 'int main() { return 1>=1; }'
assert 0 'int main() { return 1>=2; }'

assert 3 'int main() { 1; 2; return 3; }'
assert 3 'int main() { int a=3; return a; }'
assert 8 'int main() { int a=3; int z=5; return a+z; }'
assert 6 'int main() { int a; int b; a=b=3; return a+b; }'
assert 3 'int main() { int foo = 3; return foo; }'
assert 8 'int main() { int foo123=3; int bar=5; return foo123 + bar; }'

assert 1 'int main() { return 1; 2; 3; }'
assert 2 'int main() { 1; return 2; 3; }'
assert 3 'int main() { 1; 2; return 3; }'

assert 3 'int main() { {1; {2;} return 6/2;} }'
assert 5 'int main() { ;;; return 5; }'

assert 3 'int main() { if (0) return 2; return 3; }'
assert 3 'int main() { if (1-1) return 2; return 3; }'
assert 2 'int main() { if (1) return 2; return 3; }'
assert 2 'int main() { if (2-1) return 2; return 3; }'
assert 2 'int main() { if (2/1) return 6/3; return 12/4; }'
assert 3 'int main() { int a=0; if (a) return 2; return 3; }'
assert 2 'int main() { int a=1; int b=2; if (a < b) return 2; return 3; }'
assert 4 'int main() { if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 'int main() { if (1) { 1; 2; return 3; } else { return 4; } }'
assert 2 'int main() { if (1) if (0) { return 1; } else { return 2; } return 3; }'

assert 55 'int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3  'int main() { for (;;) {return 3;} return 5; }'
assert 10 'int main() { int j=0; int i; for (i=2048/2; i>2/2; i=i/2) j=j+1; return j; }'

assert 10 'int main() { int i=0; while (i<10) { i=i+1; } return i; }'
assert 1  'int main() { int i=1024; while (i > 2/2) { i=i/2; } return i; }'
assert 3 'int main() { {1; {2;} return 3;} }'

assert 3 'int main() { int x=3; return *&x; }'
assert 3 'int main() { int x=3; int *y=&x; int **z=&y; return **z; }'
assert 5 'int main() { int x=3; int y=5; return *(&x+1); }'
assert 3 'int main() { int x=3; int y=5; return *(&y-1); }'
assert 5 'int main() { int x=3; int y=5; return *(&x-(-1)); }'
assert 5 'int main() { int x=3; int *y=&x; *y=5; return x; }'
assert 7 'int main() { int x=3; int y=5; *(&x+1)=7; return y; }'
assert 7 'int main() { int x=3; int y=5; *(&y-2+1)=7; return x; }'
assert 5 'int main() { int x=3; return (&x+2)-&x+3; }'

assert 3  'int main() { return ret3(); }'
assert 5  'int main() { return ret5(); }'
assert 8  'int main() { return add(3, 5); }'
assert 2  'int main() { return sub(5, 3); }'
assert 10 'int main() { return add4(1, 2, 3, 4); }'
assert 6  'int main() { return add(1, add(2, 3)); }'
assert 6  'int main() { return add(add(1, 2), 3); }'

assert 32 'int main() { return ret32(); } int ret32() { return 32; }'
assert 1  'int main() { return echo(1); } int echo(int x) { return x; }'
assert 7  'int main() { return add2(3, 4); } int add2(int x, int y) { return x+y; }'
assert 1  'int main() { return sub2(4, 3); } int sub2(int x, int y) { return x-y; }'
assert 55 'int main() { return fib(9); } int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); }'
assert 2  'int main() { return div(4, 2); } int div(int a, int b) { return a/b; }'

assert 3 'int main() { int x[2]; int *y=&x; *y=3; return *x; }'
assert 3 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; }'
assert 4 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); }'
assert 5 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); }'

assert 0 'int main() { int x[2][3]; int *y=x; *y=0; return **x; }'
assert 1 'int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }'

assert 3 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *x; }'
assert 4 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+1); }'
assert 5 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }'
assert 5 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }'
assert 5 'int main() { int x[3]; *x=3; x[1]=4; 2[x]=5; return *(x+2); }'

assert 0 'int main() { int x[2][3]; int *y=x; y[0]=0; return x[0][0]; }'
assert 1 'int main() { int x[2][3]; int *y=x; y[1]=1; return x[0][1]; }'
assert 2 'int main() { int x[2][3]; int *y=x; y[2]=2; return x[0][2]; }'
assert 3 'int main() { int x[2][3]; int *y=x; y[3]=3; return x[1][0]; }'
assert 4 'int main() { int x[2][3]; int *y=x; y[4]=4; return x[1][1]; }'
assert 5 'int main() { int x[2][3]; int *y=x; y[5]=5; return x[1][2]; }'

assert 4  'int main() { int x; return sizeof(x); }'
assert 4  'int main() { int x; return sizeof x; }'
assert 4  'int main() { int *x; return sizeof(x); }'
assert 16 'int main() { int x[4]; return sizeof(x); }'
assert 48 'int main() { int x[3][4]; return sizeof(x); }'
assert 16 'int main() { int x[3][4]; return sizeof(*x); }'
assert 4  'int main() { int x[3][4]; return sizeof(**x); }'
assert 5  'int main() { int x[3][4]; return sizeof(**x) + 1; }'
assert 5  'int main() { int x[3][4]; return sizeof **x + 1; }'
assert 4  'int main() { int x[3][4]; return sizeof(**x + 1); }'
assert 4  'int main() { int x=1; return sizeof(x=2); }'
assert 1  'int main() { int x=1; sizeof(x=2); return x; }'

echo OK
