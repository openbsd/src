#objdump: -d
#name: MRI structured if
#as: -M

# Test MRI structured if pseudo-op.

.*:     file format .*

Disassembly of section .text:
0+000 <foo> cmpw %d1,%d0
0+002 <foo\+2> bles 0+014 <foo\+14>
0+004 <foo\+4> cmpw %d2,%d0
0+006 <foo\+6> bles 0+014 <foo\+14>
0+008 <foo\+8> cmpw %d1,%d2
0+00a <foo\+a> bles 0+010 <foo\+10>
0+00c <foo\+c> movew %d1,%d3
0+00e <foo\+e> bras 0+012 <foo\+12>
0+010 <foo\+10> movew %d2,%d3
0+012 <foo\+12> bras 0+01e <foo\+1e>
0+014 <foo\+14> cmpw %d0,%d1
0+016 <foo\+16> bgts 0+01c <foo\+1c>
0+018 <foo\+18> cmpw %d0,%d2
0+01a <foo\+1a> bles 0+01e <foo\+1e>
0+01c <foo\+1c> movew %d0,%d3
0+01e <foo\+1e> nop
