/* $OpenBSD: scanlib.c,v 1.1 2001/04/17 21:44:38 espie Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <a.out.h>
#include <link.h>

void
scan_library(fd, hdr, name)
    int fd;
    struct exec *hdr;
    const char *name;
{
	struct _dynamic			dyn;
	struct section_dispatch_table	sdt;

	printf("%s:\n", name);
	/* Assumes DYNAMIC structure starts data segment */
	if (lseek(fd, N_DATOFF(*hdr), SEEK_SET) == -1) {
		warn("%s: lseek", name);
		return;
	}
	if (read(fd, &dyn, sizeof dyn) != sizeof dyn) {
	    warn("%s: premature EOF reading _dynamic", name);
	    return;
	}

	/* Check version */
	switch (dyn.d_version) {
	default:
		warnx("%s: unsupported _DYNAMIC version: %d",
			name, dyn.d_version);
		return;
	case LD_VERSION_SUN:
	case LD_VERSION_BSD:
		break;
	}

	if (lseek(fd, (unsigned long)dyn.d_un.d_sdt + N_TXTOFF(*hdr),
	    SEEK_SET) == -1) {
		warn("%s: lseek", name);
		return;
	}
	if (read(fd, &sdt, sizeof sdt) != sizeof sdt) {
		warn("%s: premature EOF reading sdt", name);
		return;
	}

	if (sdt.sdt_sods) {
		struct sod	sod;
		off_t 		offset;
		char		entry[MAXPATHLEN];

		for (offset = sdt.sdt_sods; offset != 0; 
		    offset = sod.sod_next) {
			if (lseek(fd, offset, SEEK_SET) == -1) {
				warn("%s: bad seek to sod", name);
				return;
			}
			if (read(fd, &sod, sizeof(sod)) != sizeof(sod)) {
				warnx("%s: premature EOF reading sod", name);
				return;
			}

			if (lseek(fd, (off_t)sod.sod_name, SEEK_SET) == -1) {
				warn("%s: lseek", name);
				return;
			}
			(void)read(fd, entry, sizeof entry);
			/* make sure this is terminated */
			entry[MAXPATHLEN-1] = '\0';
			if (sod.sod_library)
				printf("\t-l%s.%d.%d\n", entry, 
				    sod.sod_major, sod.sod_minor);
			else
				printf("\t%s\n", name);
		}
	}
}
