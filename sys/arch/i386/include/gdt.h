void tss_alloc __P((struct pcb *));
void tss_free __P((struct pcb *));
void ldt_alloc __P((struct pcb *, union descriptor *, size_t));
void ldt_free __P((struct pcb *));
