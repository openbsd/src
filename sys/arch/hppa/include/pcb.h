/*	$OpenBSD: pcb.h,v 1.2 1999/02/25 17:36:53 mickey Exp $	*/


struct pcb {
	struct trapframe pcb_tf;
};


struct md_coredump {
	struct  trapframe md_tf;
}; 

