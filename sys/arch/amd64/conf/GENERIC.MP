include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS
option	CPU

cpu*		at mainbus?
ioapic*		at mainbus?
#aapic*		at pci?
