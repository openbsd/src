#objdump: -d
#name: ia64 opc-x

.*: +file format .*

Disassembly of section .text:

0000000000000000 <_start>:
   0:	04 00 00 00 01 00 	\[MLX\]       nop\.m 0x0
	...
   e:	00 00 04 00       	            break\.x 0x0
  12:	00 00 01 c0 ff ff 	\[MLX\]       nop\.m 0x0
  18:	ff ff 7f e0 ff ff 	            break\.x 0x3fffffffffffffff
  1e:	01 08 04 00 
  22:	00 00 01 00 00 00 	\[MLX\]       nop\.m 0x0
  28:	00 00 00 00 00 00 	            nop\.x 0x0
  2e:	04 00 04 00 
  32:	00 00 01 c0 ff ff 	\[MLX\]       nop\.m 0x0
  38:	ff ff 7f e0 ff ff 	            nop\.x 0x3fffffffffffffff
  3e:	05 08 04 00 
  42:	00 00 01 00 00 00 	\[MLX\]       nop\.m 0x0
  48:	00 00 00 80 00 00 	            movl r4=0x0
  4e:	00 60 04 00 
  52:	00 00 01 c0 ff ff 	\[MLX\]       nop\.m 0x0
  58:	ff ff 7f 80 f0 f7 	            movl r4=0xffffffffffffffff
  5e:	ff 6f 05 00 
  62:	00 00 01 80 90 78 	\[MLX\]       nop\.m 0x0
  68:	56 34 12 80 f0 76 	            movl r4=0x1234567890abcdef;;
  6e:	6d 66 00 00 
