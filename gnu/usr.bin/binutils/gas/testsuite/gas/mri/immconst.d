#objdump: -d
#name: MRI immediate constants
#as: -M
#source: constants.s

# Test MRI immediate constants

.*:     file format .*

Disassembly of section .text:
0+000 <foo> moveq #10,%d0
0+002 <foo\+2> moveq #10,%d0
0+004 <foo\+4> moveq #10,%d0
0+006 <foo\+6> moveq #10,%d0
0+008 <foo\+8> moveq #10,%d0
0+00a <foo\+a> moveq #10,%d0
0+00c <foo\+c> moveq #10,%d0
0+00e <foo\+e> moveq #10,%d0
0+010 <foo\+10> moveq #10,%d0
0+012 <foo\+12> moveq #97,%d0
0+014 <foo\+14> moveq #97,%d0
0+016 <foo\+16> nop
