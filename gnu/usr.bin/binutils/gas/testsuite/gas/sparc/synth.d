#as: -Av7
#objdump: -dr
#name: sparc synth

.*: +file format .*

Disassembly of section .text:
0+0000 <foo> xnor  %g1, %g0, %g2
0+0004 <foo\+4> xnor  %g1, %g0, %g1
0+0008 <foo\+8> neg  %g1, %g2
0+000c <foo\+c> neg  %g1
