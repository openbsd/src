#objdump: -dr --prefix-addresses
#name: MIPS abs

# Test the abs macro.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> bgez	\$a0,0+000c <foo\+c>
0+0004 <[^>]*> nop
0+0008 <[^>]*> neg	\$a0,\$a0
0+000c <[^>]*> bgez	\$a1,0+0018 <foo\+18>
0+0010 <[^>]*> move	\$a0,\$a1
0+0014 <[^>]*> neg	\$a0,\$a1
	...
