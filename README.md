# charmcc: A Small C Compiler

I'm following along with the commit history of [chibicc](https://github.com/rui314/chibicc), but ported from x86 to ARM (testing on Debian 10 on a Raspberry Pi 4).

## Why?

I think compilers are neat. I've [started a C compiler before](https://github.com/quells/rcc) but that petered out as the source material did not get very far. chibicc is more promising in that it can compile real applications like git, sqlite, and libpng.

I'm writing charmcc in C for several reasons:

1. I still haven't written a non-trivial program in C, so this is an opportunity to learn more about the C standard library
2. Hopefully the end result will be self-hosting
3. It's easier to copy from chibicc and merely worry about porting the instruction set
