#objdump: -s
#name: elf section1

.*: +file format .*

Contents of section .text:
Contents of section .data:
 0000 000000                               ...             
# The MIPS includes a 'section .reginfo' and such here.
#...
Contents of section A:
 0000 01010101 0101                        ......          
Contents of section B:
 0000 02020202 02                          .....           
Contents of section C:
 0000 0303                                 ..              
