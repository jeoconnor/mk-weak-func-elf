`test-double-elfpatch` modifies a set of Elf format relocatable object
files to allow linking with additional object files containing
duplicated functions. This enables a method to provide replacement
functions (test doubles) without the need to modify original sources
or avoid linking their object files.  This is done by setting the
function's bind attribute in the original object file's symbol table
to WEAK thus giving preference to the function test double.

Replacement functions are identified by being defined in distinct
files or by being explicitly labeled as such with a named section
attribute.  For example the following locates the function `func` in
the custom Elf section, `.stub` which otherwise wouldn't exist.

    #include <stdio.h>
    
    void f1() __attribute__((section(".stub")));
    
    void f1()
    {
	  printf("%s:%s\n", __FILE__, __func__);
    }

The result of compiling this file and examining the object file output
with the `readelf` utility yields:

    $ readelf -WS stub.o
    There are 15 section headers, starting at offset 0x398:
    
    Section Headers:
      [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
      [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
      [ 1] .text             PROGBITS        0000000000000000 000040 000000 00  AX  0   0  1
      [ 2] .data             PROGBITS        0000000000000000 000040 000000 00  WA  0   0  1
      [ 3] .bss              NOBITS          0000000000000000 000040 000000 00  WA  0   0  1
      [ 4] .rodata           PROGBITS        0000000000000000 000040 000011 00   A  0   0  1
      [ 5] .stub             PROGBITS        0000000000000000 000051 00002a 00  AX  0   0  1
      [ 6] .rela.stub        RELA            0000000000000000 0002a0 000060 18   I 12   5  8
      [ 7] .comment          PROGBITS        0000000000000000 00007b 00002b 01  MS  0   0  1
      [ 8] .note.GNU-stack   PROGBITS        0000000000000000 0000a6 000000 00      0   0  1
      [ 9] .note.gnu.property NOTE            0000000000000000 0000a8 000020 00   A  0   0  8
      [10] .eh_frame         PROGBITS        0000000000000000 0000c8 000038 00   A  0   0  8
      [11] .rela.eh_frame    RELA            0000000000000000 000300 000018 18   I 12  10  8
      [12] .symtab           SYMTAB          0000000000000000 000100 000168 18     13  12  8
      [13] .strtab           STRTAB          0000000000000000 000268 000036 00      0   0  1
      [14] .shstrtab         STRTAB          0000000000000000 000318 00007a 00      0   0  1
    Key to Flags:
      W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
      L (link order), O (extra OS processing required), G (group), T (TLS),
      C (compressed), x (unknown), o (OS specific), E (exclude),
      l (large), p (processor specific)
    
The section header entry, `[5]`, is created as a result of adding the
`__attribute__((section(".stub")))` addition above.  Entries in the symbol
table, not shown here, will point to this code segment rather than to
the default, `.text`, segment.

There are therefore three methods for defining function test doubles.

1. Decorating function test doubles with the `__attribute__((section(...)))` syntax.

2. If the option `-p PREFIX` is supplied as a command line argument to
   the `test-double-elfpatch` command, then functions defined in a
   files beginning with that `PREFIX` will be treated as test double
   functions.
   
3. Functions defined in files selected with the `-f FILENAME` option
   will also be treated as test double functions. This option may be
   used multiple times.

The advantage of methods 1 and 2 in a build environment is that they
both allow all the object files to be treated uniformly.  Method 3
requires keeping the original and test double files separate.


__EXAMPLE__ 

The `examples` directory contains a simple example demonstrating
`test-double-elfpatch` in action.

`main()` implemented in `main.c` calls the two functions, `f1()` and
`f2()` which are both defined in `func.c`.  `stub.c` also defines
`f1()` but labeled with `__attribute__((section("")))` feature.  The
following shows the result of first building and executing with just
`main.c` and `func.c` and then doing the same but with `stub.c` added.

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

Both examples build and execute correctly, but the second example
executes the copy of `f1()` defined in `stub.c` rather than the one in
`func.c` as in the first example.


This example was compiled with GCC.

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



