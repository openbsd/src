#objdump: -d
#name: link

# Test handling of link instruction.

.*: +file format .*

Disassembly of section .text:
0+000 <foo> linkw %fp,#0
0+004 <foo\+4> linkw %fp,#-4
0+008 <foo\+8> linkw %fp,#-32767
0+00c <foo\+c> linkw %fp,#-32768
0+010 <foo\+10> linkl %fp,#-32769
0+016 <foo\+16> linkw %fp,#32767
0+01a <foo\+1a> linkl %fp,#32768
0+020 <foo\+20> linkl %fp,#32769
