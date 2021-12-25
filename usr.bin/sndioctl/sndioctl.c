/*	$OpenBSD: sndioctl.c,v 1.17 2021/12/25 16:25:07 ratchov Exp $	*/
/*
 * Copyright (c) 2014-2020 Alexandre Ratchov <alex@caoua.org>
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
#include <errno.h>
#include <poll.h>
#include <sndio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct info {
	struct info *next;
	struct sioctl_desc desc;
	unsigned ctladdr;
#define MODE_IGNORE	0	/* ignore this value */
#define MODE_PRINT	1	/* print-only, don't change value */
#define MODE_SET	2	/* set to newval value */
#define MODE_ADD	3	/* increase current value by newval */
#define MODE_SUB	4	/* decrease current value by newval */
#define MODE_TOGGLE	5	/* toggle current value */
	unsigned mode;
	int curval, newval;
};

int cmpdesc(struct sioctl_desc *, struct sioctl_desc *);
int isdiag(struct info *);
struct info *vecent(struct info *, char *, int);
struct info *nextfunc(struct info *);
struct info *nextpar(struct info *);
struct info *firstent(struct info *, char *);
struct info *nextent(struct info *, int);
int matchpar(struct info *, char *, int);
int matchent(struct info *, char *, int);
int ismono(struct info *);
void print_node(struct sioctl_node *, int);
void print_desc(struct info *, int);
void print_num(struct info *);
void print_ent(struct info *, char *);
void print_val(struct info *, int);
void print_par(struct info *, int);
int parse_name(char **, char *);
int parse_unit(char **, int *);
int parse_val(char **, float *);
int parse_node(char **, char *, int *);
int parse_modeval(char **, int *, float *);
void dump(void);
int cmd(char *);
void commit(void);
void list(void);
void ondesc(void *, struct sioctl_desc *, int);
void onctl(void *, unsigned, unsigned);

struct sioctl_hdl *hdl;
struct info *infolist;
int i_flag = 0, v_flag = 0, m_flag = 0, n_flag = 0, q_flag = 0;

static inline int
isname(int c)
{
	return (c == '_') ||
	    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9');
}

static int
ftoi(float f)
{
	return f + 0.5;
}

/*
 * compare two sioctl_desc structures, used to sort infolist
 */
int
cmpdesc(struct sioctl_desc *d1, struct sioctl_desc *d2)
{
	int res;

	res = strcmp(d1->group, d2->group);
	if (res != 0)
		return res;
	res = strcmp(d1->node0.name, d2->node0.name);
	if (res != 0)
		return res;
	res = d1->type - d2->type;
	if (res != 0)
		return res;
	res = strcmp(d1->func, d2->func);
	if (res != 0)
		return res;
	res = d1->node0.unit - d2->node0.unit;
	if (d1->type == SIOCTL_SEL ||
	    d1->type == SIOCTL_VEC ||
	    d1->type == SIOCTL_LIST) {
		if (res != 0)
			return res;
		res = strcmp(d1->node1.name, d2->node1.name);
		if (res != 0)
			return res;
		res = d1->node1.unit - d2->node1.unit;
	}
	return res;
}

/*
 * return true of the vector entry is diagonal
 */
int
isdiag(struct info *e)
{
	if (e->desc.node0.unit < 0 || e->desc.node1.unit < 0)
		return 1;
	return e->desc.node1.unit == e->desc.node0.unit;
}

/*
 * find the selector or vector entry with the given name and channels
 */
struct info *
vecent(struct info *i, char *vstr, int vunit)
{
	while (i != NULL) {
		if ((strcmp(i->desc.node1.name, vstr) == 0) &&
		    (vunit < 0 || i->desc.node1.unit == vunit))
			break;
		i = i->next;
	}
	return i;
}

/*
 * skip all parameters with the same group, name, and func
 */
struct info *
nextfunc(struct info *i)
{
	char *str, *group, *func;

	group = i->desc.group;
	func = i->desc.func;
	str = i->desc.node0.name;
	for (i = i->next; i != NULL; i = i->next) {
		if (strcmp(i->desc.group, group) != 0 ||
		    strcmp(i->desc.node0.name, str) != 0 ||
		    strcmp(i->desc.func, func) != 0)
			return i;
	}
	return NULL;
}

/*
 * find the next parameter with the same group, name, func
 */
struct info *
nextpar(struct info *i)
{
	char *str, *group, *func;
	int unit;

	group = i->desc.group;
	func = i->desc.func;
	str = i->desc.node0.name;
	unit = i->desc.node0.unit;
	for (i = i->next; i != NULL; i = i->next) {
		if (strcmp(i->desc.group, group) != 0 ||
		    strcmp(i->desc.node0.name, str) != 0 ||
		    strcmp(i->desc.func, func) != 0)
			break;
		/* XXX: need to check for -1 ? */
		if (i->desc.node0.unit != unit)
			return i;
	}
	return NULL;
}

/*
 * return the first vector entry with the given name
 */
struct info *
firstent(struct info *g, char *vstr)
{
	char *astr, *group, *func;
	struct info *i;

	group = g->desc.group;
	astr = g->desc.node0.name;
	func = g->desc.func;
	for (i = g; i != NULL; i = i->next) {
		if (strcmp(i->desc.group, group) != 0 ||
		    strcmp(i->desc.node0.name, astr) != 0 ||
		    strcmp(i->desc.func, func) != 0)
			break;
		if (!isdiag(i))
			continue;
		if (strcmp(i->desc.node1.name, vstr) == 0)
			return i;
	}
	return NULL;
}

/*
 * find the next entry of the given vector, if the mono flag
 * is set then the whole group is searched and off-diagonal entries are
 * skipped
 */
struct info *
nextent(struct info *i, int mono)
{
	char *str, *group, *func;
	int unit;

	group = i->desc.group;
	func = i->desc.func;
	str = i->desc.node0.name;
	unit = i->desc.node0.unit;
	for (i = i->next; i != NULL; i = i->next) {
		if (strcmp(i->desc.group, group) != 0 ||
		    strcmp(i->desc.node0.name, str) != 0 ||
		    strcmp(i->desc.func, func) != 0)
			return NULL;
		if (mono)
			return i;
		if (i->desc.node0.unit == unit)
			return i;
	}
	return NULL;
}

/*
 * return true if parameter matches the given name and channel
 */
int
matchpar(struct info *i, char *astr, int aunit)
{
	if (strcmp(i->desc.node0.name, astr) != 0)
		return 0;
	if (aunit < 0)
		return 1;
	else if (i->desc.node0.unit < 0) {
		fprintf(stderr, "unit used for parameter with no unit\n");
		exit(1);
	}
	return i->desc.node0.unit == aunit;
}

/*
 * return true if selector or vector entry matches the given name and
 * channel range
 */
int
matchent(struct info *i, char *vstr, int vunit)
{
	if (strcmp(i->desc.node1.name, vstr) != 0)
		return 0;
	if (vunit < 0)
		return 1;
	else if (i->desc.node1.unit < 0) {
		fprintf(stderr, "unit used for parameter with no unit\n");
		exit(1);
	}
	return i->desc.node1.unit == vunit;
}

/*
 * return true if the given group can be represented as a signle mono
 * parameter
 */
int
ismono(struct info *g)
{
	struct info *p1, *p2;
	struct info *e1, *e2;

	p1 = g;
	switch (g->desc.type) {
	case SIOCTL_NUM:
	case SIOCTL_SW:
		for (p2 = g; p2 != NULL; p2 = nextpar(p2)) {
			if (p2->curval != p1->curval)
				return 0;
		}
		break;
	case SIOCTL_SEL:
	case SIOCTL_VEC:
	case SIOCTL_LIST:
		for (p2 = g; p2 != NULL; p2 = nextpar(p2)) {
			for (e2 = p2; e2 != NULL; e2 = nextent(e2, 0)) {
				if (!isdiag(e2)) {
					if (e2->curval != 0)
						return 0;
				} else {
					e1 = vecent(p1,
					    e2->desc.node1.name,
					    p1->desc.node0.unit);
					if (e1 == NULL)
						continue;
					if (e1->curval != e2->curval)
						return 0;
				}
			}
		}
		break;
	}
	return 1;
}

/*
 * print a sub-stream, eg. "spkr[4]"
 */
void
print_node(struct sioctl_node *c, int mono)
{
	printf("%s", c->name);
	if (!mono && c->unit >= 0)
		printf("[%d]", c->unit);
}

/*
 * print info about the parameter
 */
void
print_desc(struct info *p, int mono)
{
	struct info *e;
	int more;

	switch (p->desc.type) {
	case SIOCTL_NUM:
	case SIOCTL_SW:
		printf("*");
		break;
	case SIOCTL_SEL:
	case SIOCTL_VEC:
	case SIOCTL_LIST:
		more = 0;
		for (e = p; e != NULL; e = nextent(e, mono)) {
			if (mono) {
				if (!isdiag(e))
					continue;
				if (e != firstent(p, e->desc.node1.name))
					continue;
			}
			if (more)
				printf(",");
			print_node(&e->desc.node1, mono);
			if (p->desc.type != SIOCTL_SEL)
				printf(":*");
			more = 1;
		}
	}
}

void
print_num(struct info *p)
{
	if (p->desc.maxval == 1)
		printf("%d", p->curval);
	else {
		/*
		 * For now, maxval is always 127 or 255,
		 * so three decimals is always ideal.
		 */
		printf("%.3f", p->curval / (float)p->desc.maxval);
	}
}

/*
 * print a single control
 */
void
print_ent(struct info *e, char *comment)
{
	if (e->desc.group[0] != 0) {
		printf("%s", e->desc.group);
		printf("/");
	}
	print_node(&e->desc.node0, 0);
	printf(".%s=", e->desc.func);
	switch (e->desc.type) {
	case SIOCTL_NONE:
		printf("<removed>\n");
		break;
	case SIOCTL_SEL:
	case SIOCTL_VEC:
	case SIOCTL_LIST:
		print_node(&e->desc.node1, 0);
		printf(":");
		/* FALLTHROUGH */
	case SIOCTL_SW:
	case SIOCTL_NUM:
		print_num(e);
	}
	if (comment)
		printf("\t# %s", comment);
	printf("\n");
}

/*
 * print parameter value
 */
void
print_val(struct info *p, int mono)
{
	struct info *e;
	int more;

	switch (p->desc.type) {
	case SIOCTL_NUM:
	case SIOCTL_SW:
		print_num(p);
		break;
	case SIOCTL_SEL:
	case SIOCTL_VEC:
	case SIOCTL_LIST:
		more = 0;
		for (e = p; e != NULL; e = nextent(e, mono)) {
			if (mono) {
				if (!isdiag(e))
					continue;
				if (e != firstent(p, e->desc.node1.name))
					continue;
			}
			if (e->desc.maxval == 1) {
				if (e->curval) {
					if (more)
						printf(",");
					print_node(&e->desc.node1, mono);
					more = 1;
				}
			} else {
				if (more)
					printf(",");
				print_node(&e->desc.node1, mono);
				printf(":");
				print_num(e);
				more = 1;
			}
		}
	}
}

/*
 * print ``<parameter>=<value>'' string (including '\n')
 */
void
print_par(struct info *p, int mono)
{
	if (!n_flag) {
		if (p->desc.group[0] != 0) {
			printf("%s", p->desc.group);
			printf("/");
		}
		print_node(&p->desc.node0, mono);
		printf(".%s=", p->desc.func);
	}
	if (i_flag)
		print_desc(p, mono);
	else
		print_val(p, mono);
	printf("\n");
}

/*
 * parse a stream name or parameter name
 */
int
parse_name(char **line, char *name)
{
	char *p = *line;
	unsigned len = 0;

	if (!isname(*p)) {
		fprintf(stderr, "letter or digit expected near '%s'\n", p);
		return 0;
	}
	while (isname(*p)) {
		if (len >= SIOCTL_NAMEMAX - 1) {
			name[SIOCTL_NAMEMAX - 1] = '\0';
			fprintf(stderr, "%s...: too long\n", name);
			return 0;
		}
		name[len++] = *p;
		p++;
	}
	name[len] = '\0';
	*line = p;
	return 1;
}

/*
 * parse a decimal integer
 */
int
parse_unit(char **line, int *num)
{
	char *p = *line;
	unsigned int val;
	int n;

	if (sscanf(p, "%u%n", &val, &n) != 1) {
		fprintf(stderr, "number expected near '%s'\n", p);
		return 0;
	}
	if (val >= 255) {
		fprintf(stderr, "%d: too large\n", val);
		return 0;
	}
	*num = val;
	*line = p + n;
	return 1;
}

int
parse_val(char **line, float *num)
{
	char *p = *line;
	float val;
	int n;

	if (sscanf(p, "%g%n", &val, &n) != 1) {
		fprintf(stderr, "number expected near '%s'\n", p);
		return 0;
	}
	if (val < 0 || val > 1) {
		fprintf(stderr, "%g: expected number between 0 and 1\n", val);
		return 0;
	}
	*num = val;
	*line = p + n;
	return 1;
}

/*
 * parse a sub-stream, eg. "spkr[7]"
 */
int
parse_node(char **line, char *str, int *unit)
{
	char *p = *line;

	if (!parse_name(&p, str))
		return 0;
	if (*p != '[') {
		*unit = -1;
		*line = p;
		return 1;
	}
	p++;
	if (!parse_unit(&p, unit))
		return 0;
	if (*p != ']') {
		fprintf(stderr, "']' expected near '%s'\n", p);
		return 0;
	}
	p++;
	*line = p;
	return 1;
}

/*
 * parse a decimal prefixed by the optional mode
 */
int
parse_modeval(char **line, int *rmode, float *rval)
{
	char *p = *line;
	unsigned mode;

	switch (*p) {
	case '+':
		mode = MODE_ADD;
		p++;
		break;
	case '-':
		mode = MODE_SUB;
		p++;
		break;
	case '!':
		mode = MODE_TOGGLE;
		p++;
		break;
	default:
		mode = MODE_SET;
	}
	if (mode != MODE_TOGGLE) {
		if (!parse_val(&p, rval))
			return 0;
	}
	*line = p;
	*rmode = mode;
	return 1;
}

/*
 * dump the whole controls list, useful for debugging
 */
void
dump(void)
{
	struct info *i;

	for (i = infolist; i != NULL; i = i->next) {
		printf("%03u:", i->ctladdr);
		print_node(&i->desc.node0, 0);
		printf(".%s", i->desc.func);
		printf("=");
		switch (i->desc.type) {
		case SIOCTL_NUM:
		case SIOCTL_SW:
			printf("0..%d (%u)", i->desc.maxval, i->curval);
			break;
		case SIOCTL_SEL:
			print_node(&i->desc.node1, 0);
			break;
		case SIOCTL_VEC:
		case SIOCTL_LIST:
			print_node(&i->desc.node1, 0);
			printf(":0..%d (%u)", i->desc.maxval, i->curval);
		}
		printf("\n");
	}
}

/*
 * parse and execute a command ``<parameter>[=<value>]''
 */
int
cmd(char *line)
{
	char *pos, *group;
	struct info *i, *e, *g;
	char func[SIOCTL_NAMEMAX];
	char astr[SIOCTL_NAMEMAX], vstr[SIOCTL_NAMEMAX];
	int aunit, vunit;
	unsigned npar = 0, nent = 0;
	int comma, mode;
	float val;

	pos = strrchr(line, '/');
	if (pos != NULL) {
		group = line;
		pos[0] = 0;
		pos++;
	} else {
		group = "";
		pos = line;
	}
	if (!parse_node(&pos, astr, &aunit))
		return 0;
	if (*pos != '.') {
		fprintf(stderr, "'.' expected near '%s'\n", pos);
		return 0;
	}
	pos++;
	if (!parse_name(&pos, func))
		return 0;
	for (g = infolist;; g = g->next) {
		if (g == NULL) {
			fprintf(stderr, "%s.%s: no such control\n", astr, func);
			return 0;
		}
		if (strcmp(g->desc.group, group) == 0 &&
		    strcmp(g->desc.func, func) == 0 &&
		    strcmp(g->desc.node0.name, astr) == 0)
			break;
	}
	g->mode = MODE_PRINT;
	if (*pos != '=') {
		if (*pos != '\0') {
			fprintf(stderr, "junk at end of command\n");
			return 0;
		}
		return 1;
	}
	pos++;
	if (i_flag) {
		printf("can't set values in info mode\n");
		return 0;
	}
	npar = 0;
	switch (g->desc.type) {
	case SIOCTL_NUM:
	case SIOCTL_SW:
		if (!parse_modeval(&pos, &mode, &val))
			return 0;
		for (i = g; i != NULL; i = nextpar(i)) {
			if (!matchpar(i, astr, aunit))
				continue;
			i->mode = mode;
			i->newval = ftoi(val * i->desc.maxval);
			npar++;
		}
		break;
	case SIOCTL_SEL:
		if (*pos == '\0') {
			fprintf(stderr, "%s.%s: expects value\n", astr, func);
			exit(1);
		}
		/* FALLTROUGH */
	case SIOCTL_VEC:
	case SIOCTL_LIST:
		for (i = g; i != NULL; i = nextpar(i)) {
			if (!matchpar(i, astr, aunit))
				continue;
			for (e = i; e != NULL; e = nextent(e, 0)) {
				e->newval = 0;
				e->mode = MODE_SET;
			}
			npar++;
		}
		comma = 0;
		for (;;) {
			if (*pos == '\0')
				break;
			if (comma) {
				if (*pos != ',')
					break;
				pos++;
			}
			if (!parse_node(&pos, vstr, &vunit))
				return 0;
			if (*pos == ':') {
				pos++;
				if (!parse_modeval(&pos, &mode, &val))
					return 0;
			} else {
				val = 1.;
				mode = MODE_SET;
			}
			nent = 0;
			for (i = g; i != NULL; i = nextpar(i)) {
				if (!matchpar(i, astr, aunit))
					continue;
				for (e = i; e != NULL; e = nextent(e, 0)) {
					if (matchent(e, vstr, vunit)) {
						e->newval = ftoi(val * e->desc.maxval);
						e->mode = mode;
						nent++;
					}
				}
			}
			if (nent == 0) {
				/* XXX: use print_node()-like routine */
				fprintf(stderr, "%s[%d]: invalid value\n", vstr, vunit);
				print_par(g, 0);
				exit(1);
			}
			comma = 1;
		}
	}
	if (npar == 0) {
		fprintf(stderr, "%s: invalid parameter\n", line);
		exit(1);
	}
	if (*pos != '\0') {
		printf("%s: junk at end of command\n", pos);
		exit(1);
	}
	return 1;
}

/*
 * write the controls with the ``set'' flag on the device
 */
void
commit(void)
{
	struct info *i;
	int val;

	for (i = infolist; i != NULL; i = i->next) {
		val = 0xdeadbeef;
		switch (i->mode) {
		case MODE_IGNORE:
		case MODE_PRINT:
			continue;
		case MODE_SET:
			val = i->newval;
			break;
		case MODE_ADD:
			val = i->curval + i->newval;
			if (val > i->desc.maxval)
				val = i->desc.maxval;
			break;
		case MODE_SUB:
			val = i->curval - i->newval;
			if (val < 0)
				val = 0;
			break;
		case MODE_TOGGLE:
			val = i->curval ? 0 : i->desc.maxval;
		}
		sioctl_setval(hdl, i->ctladdr, val);
		i->curval = val;
	}
}

/*
 * print all parameters
 */
void
list(void)
{
	struct info *p, *g;

	for (g = infolist; g != NULL; g = nextfunc(g)) {
		if (g->mode == MODE_IGNORE)
			continue;
		if (i_flag) {
			if (v_flag) {
				for (p = g; p != NULL; p = nextpar(p))
					print_par(p, 0);
			} else
				print_par(g, 1);
		} else {
			if (v_flag || !ismono(g)) {
				for (p = g; p != NULL; p = nextpar(p))
					print_par(p, 0);
			} else
				print_par(g, 1);
		}
	}
}

/*
 * register a new knob/button, called from the poll() loop.  this may be
 * called when label string changes, in which case we update the
 * existing label widged rather than inserting a new one.
 */
void
ondesc(void *arg, struct sioctl_desc *d, int curval)
{
	struct info *i, **pi;
	int cmp;

	if (d == NULL)
		return;

	/*
	 * delete control
	 */
	for (pi = &infolist; (i = *pi) != NULL; pi = &i->next) {
		if (d->addr == i->desc.addr) {
			if (m_flag)
				print_ent(i, "deleted");
			*pi = i->next;
			free(i);
			break;
		}
	}

	switch (d->type) {
	case SIOCTL_NUM:
	case SIOCTL_SW:
	case SIOCTL_VEC:
	case SIOCTL_LIST:
	case SIOCTL_SEL:
		break;
	default:
		return;
	}

	/*
	 * find the right position to insert the new widget
	 */
	for (pi = &infolist; (i = *pi) != NULL; pi = &i->next) {
		cmp = cmpdesc(d, &i->desc);
		if (cmp == 0) {
			fprintf(stderr, "fatal: duplicate control:\n");
			print_ent(i, "duplicate");
			exit(1);
		}
		if (cmp < 0)
			break;
	}
	i = malloc(sizeof(struct info));
	if (i == NULL) {
		perror("malloc");
		exit(1);
	}
	i->desc = *d;
	i->ctladdr = d->addr;
	i->curval = i->newval = curval;
	i->mode = MODE_IGNORE;
	i->next = *pi;
	*pi = i;
	if (m_flag)
		print_ent(i, "added");
}

/*
 * update a knob/button state, called from the poll() loop
 */
void
onctl(void *arg, unsigned addr, unsigned val)
{
	struct info *i, *j;

	i = infolist;
	for (;;) {
		if (i == NULL)
			return;
		if (i->ctladdr == addr)
			break;
		i = i->next;
	}

	if (i->curval == val) {
		print_ent(i, "eq");
		return;
	}

	if (i->desc.type == SIOCTL_SEL) {
		for (j = infolist; j != NULL; j = j->next) {
			if (strcmp(i->desc.group, j->desc.group) != 0 ||
			    strcmp(i->desc.node0.name, j->desc.node0.name) != 0 ||
			    strcmp(i->desc.func, j->desc.func) != 0 ||
			    i->desc.node0.unit != j->desc.node0.unit)
				continue;
			j->curval = (i->ctladdr == j->ctladdr);
		}
	} else
		i->curval = val;

	if (m_flag)
		print_ent(i, "changed");
}

int
main(int argc, char **argv)
{
	char *devname = SIO_DEVANY;
	int i, c, d_flag = 0;
	struct info *g;
	struct pollfd *pfds;
	int nfds, revents;

	while ((c = getopt(argc, argv, "df:imnqv")) != -1) {
		switch (c) {
		case 'd':
			d_flag = 1;
			break;
		case 'f':
			devname = optarg;
			break;
		case 'i':
			i_flag = 1;
			break;
		case 'm':
			m_flag = 1;
			break;
		case 'n':
			n_flag = 1;
			break;
		case 'q':
			q_flag = 1;
			break;
		case 'v':
			v_flag++;
			break;
		default:
			fprintf(stderr, "usage: sndioctl "
			    "[-dimnqv] [-f device] [command ...]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	hdl = sioctl_open(devname, SIOCTL_READ | SIOCTL_WRITE, 0);
	if (hdl == NULL) {
		fprintf(stderr, "%s: can't open control device\n", devname);
		exit(1);
	}

	if (pledge("stdio audio", NULL) == -1) {
		fprintf(stderr, "%s: pledge: %s\n", getprogname(),
		    strerror(errno));
		exit(1);
	}

	if (!sioctl_ondesc(hdl, ondesc, NULL)) {
		fprintf(stderr, "%s: can't get device description\n", devname);
		exit(1);
	}
	sioctl_onval(hdl, onctl, NULL);

	if (d_flag) {
		if (argc > 0) {
			fprintf(stderr,
			    "commands are not allowed with -d option\n");
			exit(1);
		}
		dump();
	} else {
		if (argc == 0) {
			for (g = infolist; g != NULL; g = nextfunc(g))
				g->mode = MODE_PRINT;
		} else {
			for (i = 0; i < argc; i++) {
				if (!cmd(argv[i]))
					return 1;
			}
		}
		commit();
		if (!q_flag)
			list();
	}
	if (m_flag) {
		pfds = malloc(sizeof(struct pollfd) * sioctl_nfds(hdl));
		if (pfds == NULL) {
			perror("malloc");
			exit(1);
		}
		for (;;) {
                       fflush(stdout);
			nfds = sioctl_pollfd(hdl, pfds, POLLIN);
			if (nfds == 0)
				break;
			while (poll(pfds, nfds, -1) < 0) {
				if (errno != EINTR) {
					perror("poll");
					exit(1);
				}
			}
			revents = sioctl_revents(hdl, pfds);
			if (revents & POLLHUP) {
				fprintf(stderr, "disconnected\n");
				break;
			}
		}
		free(pfds);
	}
	sioctl_close(hdl);
	return 0;
}
