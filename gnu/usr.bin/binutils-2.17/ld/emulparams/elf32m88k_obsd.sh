. ${srcdir}/emulparams/elf32m88k.sh
# Force padding around .plt
DATA_PLT=
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
. ${srcdir}/emulparams/elf_obsd.sh
TRAP=0xf400fc01	# illop1
