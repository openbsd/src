/*	$OpenBSD: pcb.h,v 1.1 1998/07/07 21:32:43 mickey Exp $	*/


struct pcb {
	int i;
};


struct md_coredump {
	struct  trapframe md_tf;
}; 

