#objdump: -dr --prefix-addresses -mmips:4100
#name: MIPS 4100
#as: -mcpu=4100


.*: +file format .*mips.*

Disassembly of section \.text:
0+0000 <stuff> dmadd16	\$a0,\$a1
	...
0+000c <stuff\+0xc> madd16	\$a1,\$a2
0+0010 <stuff\+0x10> hibernate
0+0014 <stuff\+0x14> standby
0+0018 <stuff\+0x18> suspend
0+001c <stuff\+0x1c> nop
