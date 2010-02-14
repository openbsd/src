/*	$OpenBSD: pmon.c,v 1.3 2010/02/14 22:39:33 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/cpu.h>
#include <machine/pmon.h>

int	pmon_argc;
int32_t	*pmon_argv;
int32_t	*pmon_envp;

void
pmon_init(int32_t argc, int32_t argv, int32_t envp, int32_t callvec)
{
	pmon_callvec = callvec;

	pmon_argc = argc;
	/* sign extend pointers */
	pmon_argv = (int32_t *)(vaddr_t)argv;
	pmon_envp = (int32_t *)(vaddr_t)envp;
}

const char *
pmon_getarg(const int argno)
{
	if (argno < 0 || argno >= pmon_argc)
		return NULL;

	return (const char *)(vaddr_t)pmon_argv[argno];
}

const char *
pmon_getenv(const char *var)
{
	int32_t *envptr = pmon_envp;
	const char *envstr;
	size_t varlen;

	if (envptr == NULL)
		return NULL;

	varlen = strlen(var);
	while (*envptr != 0) {
		envstr = (const char *)(vaddr_t)*envptr;
		/*
		 * There is a PMON2000 bug, at least on Lemote Yeeloong,
		 * which causes it to override part of the environment
		 * pointers array with the environment data itself.
		 *
		 * This only happens on cold boot, and if the BSD kernel
		 * is loaded without symbols (i.e. no option -k passed
		 * to the boot command).
		 *
		 * Until a suitable workaround is found or the bug is
		 * fixed, ignore broken environment information and
		 * tell the user (in case this prevents us from finding
		 * important information).
		 */
		if ((vaddr_t)envstr < CKSEG1_BASE ||
		    (vaddr_t)envstr >= CKSSEG_BASE) {
			pmon_printf("WARNING! CORRUPTED ENVIRONMENT!\n");
			pmon_printf("Unable to search for %s.\n", var);
#ifdef _STANDALONE
			pmon_printf("If boot fails, power-cycle the machine.\n");
#else
			pmon_printf("If the kernel fails to identify the system"
			    " type, please boot it again with `-k' option.\n");
#endif

			/* terminate environment for further calls */
			*envptr = 0;
			break;
		}
		if (strncmp(envstr, var, varlen) == 0 &&
		    envstr[varlen] == '=')
			return envstr + varlen + 1;
		envptr++;
	}

	return NULL;
}
