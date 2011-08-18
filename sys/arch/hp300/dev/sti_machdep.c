/*	$OpenBSD: sti_machdep.c,v 1.1 2011/08/18 20:02:57 miod Exp $	*/

/*
 * Copyright (c) 2005, 2011, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <hp300/dev/diovar.h>
#include <hp300/dev/sgcvar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>
#include <hp300/dev/sti_machdep.h>

extern struct hp300_bus_space_tag hp300_mem_tag;

/* Console data */
struct sti_rom sticn_rom;
struct sti_screen sticn_scr;
bus_addr_t sticn_bases[STI_REGION_MAX];

void
sticninit()
{
	bus_space_tag_t iot;
	bus_addr_t base;
	int i;

	/*
	 * We are not interested by the *first* console pass.
	 */
	if (consolepass == 0)
		return;

	iot = &hp300_mem_tag;
	if (conscode >= SGC_SLOT_TO_CONSCODE(0))
		base = (bus_addr_t)sgc_slottopa(CONSCODE_TO_SGC_SLOT(conscode));
	else
		base =
		    (bus_addr_t)dio_scodetopa(conscode + STI_DIO_SCODE_OFFSET);

	/* sticn_bases[0] will be fixed in sti_cnattach() */
	for (i = 0; i < STI_REGION_MAX; i++)
		sticn_bases[i] = base;

	sti_cnattach(&sticn_rom, &sticn_scr, iot, sticn_bases,
	    STI_CODEBASE_M68K);

	/*
	 * Since the copyright notice could not be displayed before,
	 * display it again now.
	 */
	printf("%s\n", copyright);
}
