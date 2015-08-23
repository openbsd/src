. ${srcdir}/emulparams/elf32ppccommon.sh
# We deliberately keep the traditional OpenBSD W^X layout for both the
# old BSS-PLT and the new Secure-PLT ABI.
BSS_PLT=
OTHER_TEXT_SECTIONS="*(.glink)"
EXTRA_EM_FILE=ppc32elf
. ${srcdir}/emulparams/elf_obsd.sh
