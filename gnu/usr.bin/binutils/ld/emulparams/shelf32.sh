# Note: this parameter script is sourced by the other
# sh[l]elf(32|64).sh parameter scripts.
SCRIPT_NAME=elf
OUTPUT_FORMAT=${OUTPUT_FORMAT-"elf32-sh64"}
TEXT_START_ADDR=0x1000
STACK_ADDR='ALIGN (0x40000) + 0x40000'
MAXPAGESIZE=128
ARCH=sh
MACHINE=sh5
ALIGNMENT=8
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
EMBEDDED=yes

DATA_START_SYMBOLS='PROVIDE (___data = .);'

# If data is located right after .text (not explicitly specified),
# then we need to align it to an 8-byte boundary.
OTHER_READONLY_SECTIONS='
PROVIDE (___rodata = DEFINED (.rodata) ? .rodata : 0);
. = ALIGN (8);
'

# Make _edata and .bss aligned by smuggling in an alignment directive.
OTHER_GOT_SECTIONS='. = ALIGN (8);'

# These are for compatibility with the COFF toolchain.
ENTRY=start
CTOR_START='___ctors = .;'
CTOR_END='___ctors_end = .;'
DTOR_START='___dtors = .;'
DTOR_END='___dtors_end = .;'

# Do not use the varname=${varname-'string'} construct here; there are
# problems with that on some shells (e.g. on Solaris) where there is a bug
# that trigs when $varname contains a "}".
OTHER_SECTIONS='
 .cranges 0 : { *(.cranges) }
'

# We need to adjust sizes in the .cranges section after relaxation, so
# we need an after_allocation function, and it goes in this file.
EXTRA_EM_FILE=${EXTRA_EM_FILE-sh64elf}
