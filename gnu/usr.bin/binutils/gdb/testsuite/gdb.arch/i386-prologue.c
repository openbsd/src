void gdb1253 (void);
void gdb1338 (void);

int
main (void)
{
  gdb1253 ();
  gdb1338 ();
  return 0;
}

/* Relevant part of the prologue from symtab/1253.  */

asm(".text\n"
    "    .align 8\n"
    "gdb1253:\n"
    "    pushl %ebp\n"
    "    xorl  %ecx, %ecx\n"
    "    movl  %esp, %ebp\n"
    "    pushl %edi\n"
    "    int   $0x03\n"
    "    leave\n"
    "    ret\n");

/* Relevant part of the prologue from backtrace/1338.  */

asm(".text\n"
    "    .align 8\n"
    "gdb1338:\n"
    "    pushl %edi\n"
    "    pushl %esi\n"
    "    pushl %ebx\n"
    "    int   $0x03\n"
    "    popl  %ebx\n"
    "    popl  %esi\n"
    "    popl  %edi\n"
    "    ret\n");
