#objdump: -d
#name: bitfield

# Test handling of bitfield instruction operands.

.*: +file format .*

Disassembly of section .text:
0+000 <foo> bfexts %a0@,1,2,%d0
0+004 <foo\+4> bfexts %a0@,1,6,%d0
0+008 <foo\+8> bfexts %a0@,3,2,%d0
0+00c <foo\+c> bfexts %a0@,3,6,%d0
0+010 <foo\+10> bfexts %a0@,1,2,%d0
0+014 <foo\+14> bfexts %a0@,1,6,%d0
0+018 <foo\+18> bfexts %a0@,3,2,%d0
0+01c <foo\+1c> bfexts %a0@,3,6,%d0
0+020 <foo\+20> bfset %a0@,1,2
0+024 <foo\+24> bfset %a0@,1,6
0+028 <foo\+28> bfset %a0@,3,2
0+02c <foo\+2c> bfset %a0@,3,6
0+030 <foo\+30> bfset %a0@,1,2
0+034 <foo\+34> bfset %a0@,1,6
0+038 <foo\+38> bfset %a0@,3,2
0+03c <foo\+3c> bfset %a0@,3,6
0+040 <foo\+40> bfexts %a0@,%d1,%d2,%d0
0+044 <foo\+44> bfexts %a0@,%d1,%d2,%d0
0+048 <foo\+48> bfset %a0@,%d1,%d2
0+04c <foo\+4c> bfset %a0@,%d1,%d2
