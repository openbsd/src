#!/bin/sh -

umask 007

PAGE_SIZE=`sysctl -n hw.pagesize`
PAD=$1
GAPDUMMY=$2

RANDOM1=$((RANDOM % (3 * PAGE_SIZE)))
RANDOM2=$((RANDOM % PAGE_SIZE))
RANDOM3=$((RANDOM % PAGE_SIZE))
RANDOM4=$((RANDOM % PAGE_SIZE))
RANDOM5=$((RANDOM % PAGE_SIZE))

cat > gap.link << __EOF__

PHDRS {
	text PT_LOAD FILEHDR PHDRS;
	rodata PT_LOAD;
	data PT_LOAD;
	bss PT_LOAD;
}

SECTIONS {
	.text : ALIGN($PAGE_SIZE) {
		LONG($PAD);
		. += $RANDOM1;
		. = ALIGN($PAGE_SIZE);
		endboot = .;
		PROVIDE (endboot = .);
		. = ALIGN($PAGE_SIZE);
		. += $RANDOM2;
		. = ALIGN(16);
		*(.text .text.*)
	} :text =$PAD

	.rodata : {
		LONG($PAD);
		. += $RANDOM3;
		. = ALIGN(16);
		*(.rodata .rodata.*)
	} :rodata =$PAD

	.data : {
		LONG($PAD);
		. = . + $RANDOM4;	/* fragment of page */
		. = ALIGN(16);
		*(.data .data.*)
	} :data =$PAD

	.bss : {
		. = . + $RANDOM5;	/* fragment of page */
		. = ALIGN(16);
		*(.bss .bss.*)
	} :bss

	note.ABI-tag 0 : { *(.note.ABI-tag) }
	.MIPS.options : { *(.MIPS.options) }
}
__EOF__

ld -r gap.link $GAPDUMMY -o gap.o
