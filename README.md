`test-double-elfpatch` modifies a set of Elf format relocatable object
files to allow linking with additional object files containing
duplicated functions. This enables a method to provide replacement
functions (test doubles) without the need to modify original sources
or avoid linking their object files.  This is done by setting the
function's bind attribute in the original object file's symbol table
to WEAK permitting giving preference to the function test double.

Replacement functions are identified by being defined in distinct
files or by being explicitly labeled as such with a named section
attribute.  For example (this works for C/C++ in both GCC and Clang)

    void func() __attribute__((section("test-double")));

The choice of the section name defined between the double quotes above
is arbitrary but must not be the name of an existing Elf section
header.  Section headers can be displayed with the `readelf`
command. This example was compiled with GCC.

    $ readelf -S func.o
    There are 16 section headers, starting at offset 0x31c:
    
    Section Headers:
      [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
      [ 0]                   NULL            00000000 000000 000000 00      0   0  0
      [ 1] .group            GROUP           00000000 000034 000008 04     13  13  4
      [ 2] .text             PROGBITS        00000000 00003c 00002f 00  AX  0   0  1
      [ 3] .rel.text         REL             00000000 000254 000020 08   I 13   2  4
      [ 4] .data             PROGBITS        00000000 00006b 000000 00  WA  0   0  1
      [ 5] .bss              NOBITS          00000000 00006b 000000 00  WA  0   0  1
      [ 6] .rodata           PROGBITS        00000000 00006b 00000e 00   A  0   0  1
      [ 7] .text.__x86.get_p PROGBITS        00000000 000079 000004 00 AXG  0   0  1
      [ 8] .comment          PROGBITS        00000000 00007d 00002b 01  MS  0   0  1
      [ 9] .note.GNU-stack   PROGBITS        00000000 0000a8 000000 00      0   0  1
      [10] .note.gnu.propert NOTE            00000000 0000a8 00001c 00   A  0   0  4
      [11] .eh_frame         PROGBITS        00000000 0000c4 000050 00   A  0   0  4
      [12] .rel.eh_frame     REL             00000000 000274 000010 08   I 13  11  4
      [13] .symtab           SYMTAB          00000000 000114 000100 10     14  12  4
      [14] .strtab           STRTAB          00000000 000214 00003e 00      0   0  1
      [15] .shstrtab         STRTAB          00000000 000284 000095 00      0   0  1
    Key to Flags:
      W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
      L (link order), O (extra OS processing required), G (group), T (TLS),
      C (compressed), x (unknown), o (OS specific), E (exclude),
      p (processor specific)

On Ubuntu the `readelf` command is found in the `binutils` package.

The `examples` directory contains a simple example demonstrating
`test-double-elfpatch` in action.

`main()` defined in `main.c` calls the two functions, `f1()` and
`f2()` which are both defined in `func.c`.  `stub.c` also defines
`f1()` but labeled with `__attribute__((section("")))`.  The following
shows the result of first building and executing with `main.c` and
`func.c` and then doing the same bit with `stub()` included.

    $ make
    *** Building ex1 ***
    rm -f *.o ex1 ex2
    cc    -c -o main.o main.c
    cc    -c -o func.o func.c
    cc     -o ex1 main.o func.o
    *** Executing ex1 
    main.c:main
    func.c:f1
    func.c:f2
    OK
    *** Building ex2 ***
    rm -f *.o ex1 ex2
    cc    -c -o main.o main.c
    cc    -c -o func.o func.c
    cc    -c -o stub.o stub.c
    ../test-double-elfpatch -w -s .stub main.o func.o stub.o
    adding f1 to function list
    cc     -o ex2 main.o func.o stub.o
    *** Executing ex2 
    main.c:main
    stub.c:f1
    func.c:f2
    OK

In the first case the instances of both `f1()` and `f2()` defined in
`func.c` are executed.  In the second example, the copy of `f1()`
defined in `stub.c` is called instead.

