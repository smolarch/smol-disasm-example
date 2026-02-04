# SmolArch disassembler example

This repository contains a simple disassembler for SmolArch. The disassembler
is built on top of generated C files from `smolarch` program.

# Building

```
meson setup build
meson compile -C build
```

# Running

Disassemble builtin test data.

```
$ ./build/smol-disasm
test data:
     0: 8108        ╭seqb       a0,a0
     2: 8bff        │addi       -1
     4: 87ff        │sori       2047
     6: 87ff        │sori       2047
     8: a029        │add        a1
     a: 212a        ╰add        a2,2
     c: 0000         illop
     e: 97ff        ╭bal        -1
    10: 87ff        │sori       2047
    12: fc08 0009   ╰breakz.eq  a0,a1
    16: 052a         mv         a1,a2
    18: 1d01         li         a0,1
    1a: 9d02        ╭li         a0,2
    1c: 0003        ╰sori15     3
    1e: 9d04        ╭li         a0,4
    20: 8005        │sori15     5
    22: 0006        ╰sori15     6
    24: 9d10        ╭li         a0,-16
    26: ffff        │sori15     32767
    28: 7fff        ╰sori15     32767
```

Disassemble a file.

```
$ ./build/smol-disasm test.bin
file: test.bin
     0: 0000         illop
     2: 8108        ╭seqb       a0,a0
     4: 8bff        │addi       -1
     6: 87ff        │sori       2047
     8: 87ff        │sori       2047
     a: a029        │add        a1
     c: 212a        ╰add        a2,2
     e: 0000         illop
```
