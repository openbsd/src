include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS
option	CPU

cpu*		at mainbus? apid ?
ioapic*		at mainbus? apid ?
#aapic*		at pci? dev ? function ?
