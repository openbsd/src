# Various syntaxes
	inb	%dx
	outl	%eax,%dx
# For these two, fix up the test case to check what modes are used -- they
# should be using outb and inw.  Currently the assembler is getting them
# both wrong.
	out	%al, $42
	in	$13, %ax
# These are used in AIX.
	inw	(%dx)
	outw	(%dx)
