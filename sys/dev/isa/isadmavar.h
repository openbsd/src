/*	$NetBSD: isadmavar.h,v 1.2 1994/10/27 04:17:09 cgd Exp $	*/

void isa_dmacascade __P((int));
void isa_dmastart __P((int, caddr_t, vm_size_t, int));
void isa_dmaabort __P((int));
void isa_dmadone __P((int, caddr_t, vm_size_t, int));
