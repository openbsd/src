#objdump: -dr
#name: flag

# Test the flag macro.

.*: +file format elf32-.*arc

No symbols in "a.out".
Disassembly of section .text:
00000000 1fa00000	flag r0
00000004 1fbf8001	flag 1
00000008 1fbf8002	flag 2
0000000c 1fbf8004	flag 4
00000010 1fbf8008	flag 8
00000014 1fbf8010	flag 16
00000018 1fbf8020	flag 32
0000001c 1fbf8040	flag 64
00000020 1fbf8080	flag 128
00000024 1fbf0000	flag -2147483647
0000002c 1fa0000b	flag.lt r0
00000030 1fbf0009	flag.gt 1
00000038 1fbf0009	flag.gt 2
00000040 1fbf0009	flag.gt 4
00000048 1fbf0009	flag.gt 8
00000050 1fbf0009	flag.gt 16
00000058 1fbf0009	flag.gt 32
00000060 1fbf0009	flag.gt 64
00000068 1fbf0009	flag.gt 128
00000070 1fbf000a	flag.ge -2147483647
