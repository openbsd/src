/* $OpenBSD: scanlib.c,v 1.4 2002/07/19 19:28:12 marc Exp $ */

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
scan_library(int fd, struct exec *hdr, const char *name, const char *fmt1,
	     const char *fmt2)
{
	struct _dynamic			dyn;
	struct section_dispatch_table	sdt;
	const char *fmt;
	char c;

	if (!fmt1 && !fmt2)
		printf("%s:\n", name);
	if (!fmt1)
		fmt1="\t-l%o.%m => %p (%x)\n";
	if (!fmt2)
		fmt2="\t%o (%x)\n";
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
		off_t		offset;
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
				fmt = fmt1;
			else
				fmt = fmt2;
			while ((c = *fmt++) != '\0') {
				switch(c) {
				default:
					putchar(c);
					continue;
				case '\\':
					switch (c = *fmt) {
					case '\0':
						continue;
					case 'n':
						putchar('\n');
						break;
					case 't':
						putchar('\t');
						break;
					}
					fmt++;
					break;
				case '%':
					switch (c = *fmt) {
					case '\0':
						continue;
					case '%':
					default:
						putchar(c);
						break;
					case 'A':
						printf("%s", name);
						break;
					case 'a':
						printf("%s", name);
						break;
					case 'o':
						printf("%s", entry);
						break;
					case 'm':
						printf("%d", sod.sod_major);
						break;
					case 'n':
						printf("%d", sod.sod_minor);
						break;
					case 'p':
						putchar('?');
						break;
					case 'x':
						putchar('?');
						break;
					}
					++fmt;
					break;
				}
			}

		}
	}
}
