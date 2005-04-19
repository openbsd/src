/*	$OpenBSD: prom_machdep.c,v 1.1 2005/04/19 21:30:18 miod Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
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
 */

/*
 * Routines to hide the Solbourne PROM specifics.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>	/* romboot() prototype */
#include <machine/idt.h>
#include <machine/kap.h>
#include <machine/prom.h>

#include <uvm/uvm_extern.h>

#include <sparc/sparc/asm.h>

int	sysmodel;

void	myetheraddr(u_char *);
void	prom_map(void);
void	prom_unmap(void);

extern void tlb_flush_all(void);

/*
 * Lookup a variable in the environment strings.
 */
const char *
prom_getenv(const char *var)
{
	u_int i;
	const char *eq, *env;
	size_t len;
	extern char **prom_environ;

	len = strlen(var);

	for (i = 0; (env = prom_environ[i]) != NULL; i++) {
		eq = strchr(env, '=');
#ifdef DIAGNOSTIC
		if (eq == NULL)
			continue;	/* can't happen */
#endif
		if (eq - env != len)
			continue;

		if (strncasecmp(var, env, len) == 0) {
			return (eq + 1);
		}
	}

	return (NULL);
}

void
myetheraddr(u_char *cp)
{
	const char *enetaddr;
	int i;

	enetaddr = prom_getenv(ENV_ETHERADDR);
	if (enetaddr == NULL) {
		cp[0] = cp[1] = cp[2] = cp[3] = cp[4] = cp[5] = 0xff;
	} else {
		for (i = 0; i < 6; i++) {
			cp[i] = 0;
			for (;;) {
				if (*enetaddr >= '0' && *enetaddr <= '9')
					cp[i] = cp[i] * 0x10 +
					    (*enetaddr - '0');
				else if (*enetaddr >= 'A' && *enetaddr <= 'F')
					cp[i] = cp[i] * 0x10 +
					    (*enetaddr + 10 - 'A');
				else if (*enetaddr >= 'a' && *enetaddr <= 'f')
					cp[i] = cp[i] * 0x10 +
					    (*enetaddr + 10 - 'a');
				else
					break;

				enetaddr++;
			}
			if (*enetaddr++ != ':')
				break;
		}
		/* fill remaining digits if necessary */
		while (i++ < 6)
			cp[i] = 0;
	}
}

/*
 * Set up PROM-friendly mappings
 */

void
prom_map(void)
{
	sta(0, ASI_PTW0, PTW0_DEFAULT);
	tlb_flush_all();
}

void
prom_unmap(void)
{
	sta(0, ASI_PTW0, 0);
	tlb_flush_all();
}

/*
 * Prom property access
 *
 * Note that if we are passed addresses in the fd va window, we need to
 * temporarily copy these pointers to a ``safe'' address (in this case,
 * a global variable, thus in the f0 or f1 window).
 */

int
getprop(int node, char *name, void *buf, int bufsiz)
{
	struct prom_node *n = (struct prom_node *)node;
	struct prom_prop *p;
	char *propname, *eq;
	int len, proplen;

	len = strlen(name);

#ifdef DIAGNOSTIC
	if (node == 0)
#if 0
		node = findroot();
#else
		panic("getprop(%s) invoked on invalid node", name);
#endif
#endif

	for (p = (struct prom_prop *)n->pn_props; p != NULL; p = p->pp_next) {
		propname = p->pp_data;
		eq = strchr(propname, '=');
#ifdef DIAGNOSTIC
		if (eq == NULL)
			continue;	/* can't happen */
#endif
		if (eq - propname != len)
			continue;

		if (strncmp(name, propname, len) == 0) {
			proplen = p->pp_size;
			if (proplen > bufsiz) {
				printf("node %p property %s length %d > %d",
				    node, name, proplen, bufsiz);
#ifdef DEBUG
				panic("getprop");
#else
				return (0);
#endif
			} else
				bcopy(eq + 1, buf, proplen);
			break;
		}
	}
		
	if (p == NULL)
		proplen = -1;

	return (proplen);
}

int
getproplen(int node, char *name)
{
	struct prom_node *n = (struct prom_node *)node;
	struct prom_prop *p;
	char *propname, *eq;
	int len, proplen;

#ifdef DIAGNOSTIC
	if (node == 0)
		panic("getproplen(%s) invoked on invalid node", name);
#endif

	len = strlen(name);

	for (p = (struct prom_prop *)n->pn_props; p != NULL; p = p->pp_next) {
		propname = p->pp_data;
		eq = strchr(propname, '=');
#ifdef DIAGNOSTIC
		if (eq == NULL)
			continue;	/* can't happen */
#endif
		if (eq - propname != len)
			continue;

		if (strncmp(name, propname, len) == 0) {
			proplen = p->pp_size;
			break;
		}
	}
		
	if (p == NULL)
		proplen = -1;

	return (proplen);
}

int
firstchild(int node)
{
	return ((struct prom_node *)node)->pn_child;
}

int
nextsibling(int node)
{
	if (node == 0)
		return (findroot());
	else
		return (((struct prom_node *)node)->pn_sibling);
}

int
findroot()
{
	struct sb_prom *sp;

	sp = (struct sb_prom *)PROM_DATA_VA;
	if (sp->sp_interface >= PROM_INTERFACE)
		return (sp->sp_rootnode);

	panic("findroot: PROM communication interface is too old (%d)",
	    sp->sp_interface);
	/* NOTREACHED */
}

/*
 * Shutdown and reboot interface
 */

void
romhalt()
{
	struct sb_prom *sp;

	sp = (struct sb_prom *)PROM_DATA_VA;
	if (sp->sp_interface >= PROM_INTERFACE) {
		prom_map();
		(*sp->sp_interp)("reset " PROM_RESET_HALT);
		prom_unmap();
	}

	panic("PROM exit failed");
}

void
romboot(char *str)
{
	char command[256];
	struct sb_prom *sp;

	if (*str != '\0') {
		strlcpy(command, "boot ", sizeof command);
		strlcat(command, str, sizeof command);
	} else {
		strlcpy(command, "reset ", sizeof command);
		strlcat(command, PROM_RESET_WARM, sizeof command);
	}

	sp = (struct sb_prom *)PROM_DATA_VA;
	if (sp->sp_interface >= PROM_INTERFACE) {
		prom_map();
		(*sp->sp_interp)(command);
		prom_unmap();
	}

	panic("PROM boot failed");
}
