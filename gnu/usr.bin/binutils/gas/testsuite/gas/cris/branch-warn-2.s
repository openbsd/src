; Test warning for expansion of branches.
; FIXME: Warnings currently have the line number of the last
; line, which is not really good.

;  { dg-do assemble { target cris-*-* } }
;  { dg-options "-N" }

 .text
start:
 ba long_forward ; { dg-warning "32-bit conditional branch generated" "" { target cris-*-* } { 13 } }
 .space 32768,0
long_forward:
 nop
