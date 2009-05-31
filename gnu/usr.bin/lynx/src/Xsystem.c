/* $Id: Xsystem.c,v 1.4 2009/05/31 09:16:52 avsm Exp $
 *	like system("cmd") but return with exit code of "cmd"
 *	for Turbo-C/MS-C/LSI-C
 *  This code is in the public domain.
 *
 * $Log: Xsystem.c,v $
 * Revision 1.4  2009/05/31 09:16:52  avsm
 * Update to lynx-2.8.6.rel5, with our local patches maintained where relevant.
 * tests from miod@ sthen@ jmc@ jsing@
 * two additional fixes from miod:
 * - fix uninitialized stack variable use, leading to occasional crash.
 * - modify the socklen_t test to include <sys/types.h>, fixes gcc2 build failures
 *
 *
 * Revision 1.14  1997/10/17 (Fri) 16:28:24  senshu
 * *** for Win32 version ***
 *
 * Revision 1.13  1992/02/24  06:59:13  serow
 * *** empty log message ***
 *
 * Revision 1.12  1991/04/09  08:48:20  serow
 * ignore new line at command line tail
 *
 * Revision 1.11  1991/03/12  07:12:50  serow
 * CMDLINE
 *
 * Revision 1.10  91/02/24  05:10:14  serow
 * 2>&1
 *
 * Revision 1.9  91/02/22  07:01:17  serow
 * NEAR for ms-c
 *
 */
#include <LYUtils.h>
#include <LYStrings.h>

#ifdef DOSPATH
#include <io.h>
#else
extern char *mktemp(char *);
#endif

#ifndef USECMDLINE
#define USECMDLINE	0
#endif

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

#define	TABLESIZE(v)	(sizeof(v)/sizeof(v[0]))

#define STR_MAX 512		/* MAX command line */

#define isk1(c)  ((0x81 <= UCH(c) && UCH(c) <= 0x9F) || (0xE0 <= UCH(c) && UCH(c) <= 0xFC))
#define isq(c)   ((c) == '"')
#define isspc(c) ((c) == ' ' || (c) == '\t')
#define issep(c) (isspc(c) || (c) == '"' || (c) == '\'' || (c) == '<' || (c) == '>' || (c) == 0)
#define issep2(c) (issep(c) || (c) == '.' || (c) == '\\' || (c) == '/')
#define isdeg(c) ('0' <= (c) && (c) <= '9')

#ifndef NEAR
#if 0				/* MS-C */
#define NEAR	_near
#else
#define NEAR
#endif
#endif

typedef struct _proc {
    struct _proc *next;
    char *line;
    char *cmd;
    char *arg;
    char *inf;
    int infmod;
    char *outf;
    int outfmod;
    int ored[10];
    int sred[10];
} PRO;

static PRO *p1 = 0;

static char *NEAR xmalloc(size_t n)
{
    char *bp;

    if ((bp = typecallocn(char, n)) == 0) {
	write(2, "xsystem: Out of memory.!\n", 25);
	exit_immediately(1);
    }
    return bp;
}

static char *NEAR xrealloc(void *p, size_t n)
{
    char *bp;

    if ((bp = realloc(p, n)) == (char *) 0) {
	write(2, "xsystem: Out of memory!.\n", 25);
	exit_immediately(1);
    }
    return bp;
}

static int NEAR is_builtin_command(char *s)
{
#ifdef WIN_EX
    extern int system_is_NT;	/* 1997/11/05 (Wed) 22:10:35 */
#endif

    static char *cmdtab[] =
    {
	"dir", "type", "rem", "ren", "rename", "erase", "del",
	"copy", "pause", "date", "time", "ver", "vol", "label",
	"cd", "chdir", "md", "mkdir", "rd", "rmdir", "break",
	"verify", "set", "prompt", "path", "exit", "ctty", "echo",
	"if", "for", "cls", "goto", "shift"
	,"start"		/* start is NT only */
    };
    int i, l, lc, count;

    l = strlen(s);
    count = TABLESIZE(cmdtab);
    count--;
#ifdef WIN_EX
    if (system_is_NT)
	count++;
#endif
    for (i = 0; i < count; i++) {
	if (strcasecomp(s, cmdtab[i]) == 0)
	    return 1;
	lc = strlen(cmdtab[i]);
	if (lc < l && strnicmp(s, cmdtab[i], lc) == 0 && issep2(s[lc]))
	    return 1;
    }
    return 0;
}

static int NEAR getswchar(void)
{
#ifdef __WIN32__
    return '/';
#else
    union REGS reg;

    reg.x.ax = 0x3700;
    intdos(&reg, &reg);
    return reg.h.dl;
#endif
}

static int NEAR csystem(PRO * p, int flag)
{
    char *cmp;
    char SW[3];
    int rc;

    if ((cmp = LYGetEnv("COMSPEC")) == 0)
	return -2;
    SW[0] = (char) getswchar();
    SW[1] = 'c';
    SW[2] = 0;
    rc = spawnl(flag, cmp, cmp, SW, p->cmd, p->arg, (char *) 0);
    return rc < 0 ? -2 : rc;
}

static PRO *NEAR pars1c(char *s)
{
    PRO *pp;
    char *fnp;
    int ms, mi;
    int fs, fi, inpf;
    int q;

    pp = (PRO *) xmalloc(sizeof(PRO));
    for (q = 0; q < TABLESIZE(pp->ored); q++)
	pp->ored[q] = q;
    while (isspc(*s))
	s++;
    pp->line = strdup(s);
    pp->cmd = xmalloc(ms = 8);
    mi = 0;
    while (!issep(*s)) {
	if (mi >= ms - 1)
	    pp->cmd = xrealloc(pp->cmd, ms += 8);
	pp->cmd[mi++] = *s++;
    }
    pp->cmd[mi] = 0;
    q = 0;
    pp->arg = xmalloc(ms = 32);
    if (isspc(*s))
	s++;
    mi = 0;
    while (*s) {
	if (mi >= ms - 1) {
	    pp->arg = xrealloc(pp->arg, ms += 32);
	}
	if (q == 0) {
	    inpf = 0;
	    if ((mi == 0 || isspc(s[-1])) &&
		isdeg(s[0]) && s[1] == '>' &&
		s[2] == '&' && isdeg(s[3])) {

		pp->ored[s[0] & 15] = s[3] & 15;
		s += 4;
		continue;
	    } else if (s[0] == '<') {
		if (pp->inf == 0) {
		    pp->infmod = O_RDONLY;
		}
		inpf = 1;
	    } else if (s[0] == '>' && s[1] == '>') {
		if (pp->outf == 0) {
		    pp->outfmod = O_WRONLY | O_CREAT | O_APPEND;
		}
		s++;
	    } else if (s[0] == '>') {
		if (pp->outf == 0) {
		    pp->outfmod = O_WRONLY | O_CREAT | O_TRUNC;
		}
	    } else {
		if (*s == '"')
		    q = !q;
		pp->arg[mi++] = *s++;
		continue;
	    }
	    fnp = xmalloc(fs = 16);
	    fi = 0;
	    s++;
	    while (isspc(*s))
		s++;
	    while (!issep(*s)) {
		if (fi >= fs - 1)
		    fnp = xrealloc(fnp, fs += 16);
		fnp[fi++] = *s++;
	    }
	    fnp[fi] = 0;
	    if (inpf) {
		if (pp->inf == 0)
		    pp->inf = fnp;
	    } else {
		if (pp->outf == 0)
		    pp->outf = fnp;
	    }
	} else if (s[0] == '"') {
	    q = !q;
	    pp->arg[mi++] = *s++;
	} else {
	    pp->arg[mi++] = *s++;
	}
    }
    pp->arg[mi] = 0;
    return pp;
}

static PRO *NEAR pars(char *s)
{
    char *lb;
    int li, ls, q;
    int c;
    PRO *pp;

    lb = xmalloc(ls = STR_MAX);	/* about */
    li = q = 0;
    p1 = 0;

    for (;;) {
	c = *s++;
	if (li >= ls - 2)
	    lb = xrealloc(lb, ls += STR_MAX);
	if (isk1(c) && *s) {
	    lb[li++] = (char) c;
	    lb[li++] = *s++;
	} else if ((!q && c == '|') || c == 0 || (c == '\n' && *s == 0)) {
	    lb[li++] = 0;
	    if (p1 == 0) {
		pp = p1 = pars1c(lb);
	    } else {
		pp->next = pars1c(lb);
		pp = pp->next;
	    }
	    li = 0;
	    if (c == 0 || (c == '\n' && *s == 0))
		break;
	} else if (c == '"') {
	    q = !q;
	    lb[li++] = (char) c;
	} else {
	    lb[li++] = (char) c;
	}
    }
    free(lb);
    return p1;
}

static int NEAR try3(char *cnm, PRO * p, int flag)
{
    char cmdb[STR_MAX];
    int rc;

    sprintf(cmdb, "%.*s.com", sizeof(cmdb) - 5, cnm);
    if ((rc = open(cmdb, O_RDONLY)) >= 0) {
	close(rc);
	return spawnl(flag, cmdb, cmdb, p->arg, (char *) 0);
    }
    sprintf(cmdb, "%.*s.exe", sizeof(cmdb) - 5, cnm);
    if ((rc = open(cmdb, O_RDONLY)) >= 0) {
	close(rc);
	return spawnl(flag, cmdb, cmdb, p->arg, (char *) 0);
    }
    sprintf(cmdb, "%.*s.bat", sizeof(cmdb) - 5, cnm);
    if ((rc = open(cmdb, O_RDONLY)) >= 0) {
	close(rc);
	return csystem(p, flag);
    }
    return -1;
}

static int NEAR prog_go(PRO * p, int flag)
{
    char *s;
    char *extp = 0;
    char cmdb[STR_MAX];
    char *ep;
    int rc, lc, cmd_len;

    cmd_len = strlen(p->cmd);

    s = p->cmd + cmd_len - 1;
    while (cmd_len && (*s != '\\') && (*s != '/') && (*s != ':')) {
	if (*s == '.')
	    extp = s;
	cmd_len--;
	s--;
    }

    if (is_builtin_command(p->cmd) || (extp && strcasecomp(extp, ".bat") == 0))
	return csystem(p, flag);

    if (s < p->cmd) {		/* cmd has no PATH nor Drive */
	ep = LYGetEnv("PATH");
	LYstrncpy(cmdb, p->cmd, sizeof(cmdb) - 1);
	for (;;) {
	    if (extp) {		/* has extension */
		if ((rc = open(cmdb, O_RDONLY)) >= 0) {
		    close(rc);
		    rc = spawnl(flag, cmdb, cmdb, p->arg, (char *) 0);
		}
	    } else {
		rc = try3(cmdb, p, flag);
	    }
	    if (rc >= 0)
		return rc;

	    if (ep && *ep) {
		int i;

		for (i = 0; *ep != ';' && *ep != '\0'; ep++, i++)
		    lc = cmdb[i] = *ep;
		if (*ep == ';')
		    ep++;
		if (i > 0 && lc != ':' && lc != '\\' && lc != '/')
		    cmdb[i++] = '\\';
		cmdb[i] = 0;
		LYstrncpy(cmdb + i, p->cmd, sizeof(cmdb) - 1 - i);
	    } else {
		if (rc == -2)
		    return rc;
		return -1;
	    }
	}
    } else {			/* has PATH or Drive */
	if (extp) {		/* has extension */
	    if ((rc = open(p->cmd, O_RDONLY)) >= 0) {
		close(rc);
		return spawnl(flag, p->cmd, p->cmd, p->arg, (char *) 0);
	    }
	    return -1;
	} else {
	    return try3(p->cmd, p, flag);
	}
    }
}

static char *NEAR tmpf(char *tp)
{
    char tplate[STR_MAX];
    char *ev;
    int i;

    if ((ev = LYGetEnv("TMP")) != 0) {
	LYstrncpy(tplate, ev, sizeof(tplate) - 2 - strlen(tp));
	i = strlen(ev);
	if (i && ev[i - 1] != '\\' && ev[i - 1] != '/')
	    strcat(tplate, "\\");
    } else {
	tplate[0] = 0;
    }
    strcat(tplate, tp);
    return strdup(mktemp(tplate));
}

static int NEAR redopen(char *fn, int md, int sfd)
{
    int rc;
    int fd;

    if ((fd = open(fn, md, 0666)) != -1) {
	if (md & O_APPEND)
	    lseek(fd, 0L, SEEK_END);
	rc = dup(sfd);
	if (fd != sfd) {
	    dup2(fd, sfd);
	    close(fd);
	}
	return rc;
    }
    return -1;
}

static int NEAR redclose(int fd, int sfd)
{
    if (fd != -1) {
	dup2(fd, sfd);
	close(fd);
    }
    return -1;
}

static void NEAR redswitch(PRO * p)
{
    int d;

    for (d = 0; d < TABLESIZE(p->ored); d++) {
	if (d != p->ored[d]) {
	    p->sred[d] = dup(d);
	    dup2(p->ored[d], d);
	}
    }
}

static void NEAR redunswitch(PRO * p)
{
    int d;

    for (d = 0; d < TABLESIZE(p->ored); d++) {
	if (d != p->ored[d]) {
	    dup2(p->sred[d], d);
	    close(p->sred[d]);
	}
    }
}

int xsystem(char *cmd)
{
    PRO *p, *pn;
    char *pof, *pif, *pxf;
    int psstdin, psstdout;
    int rdstdin, rdstdout;
    int rc = 0;
    static char *cmdline = 0;

#ifdef SH_EX			/* 1997/11/01 (Sat) 10:04:03 add by JH7AYN */
    pif = cmd;
    while (*pif++) {
	if (*pif == '\r') {
	    *pif = '\0';
	    break;
	} else if (*pif == '\n') {
	    *pif = '\0';
	    break;
	}
    }
#endif

    pof = pif = pxf = 0;
    p = pars(cmd);
    pof = tmpf("p1XXXXXX");
    pif = tmpf("p2XXXXXX");
    psstdin = psstdout = rdstdin = rdstdout = -1;
    while (p) {
#if USECMDLINE
	if (!LYGetEnv("NOCMDLINE")) {
	    cmdline = xmalloc(strlen(p->cmd) + strlen(p->arg) + 10);
	    sprintf(cmdline, "CMDLINE=%s %s", p->cmd, p->arg);
	    putenv(cmdline);
	}
#endif
	if (p->next)
	    psstdout = redopen(pof, O_WRONLY | O_CREAT | O_TRUNC, 1);
	if (p->inf)
	    rdstdin = redopen(p->inf, p->infmod, 0);
	if (p->outf)
	    rdstdout = redopen(p->outf, p->outfmod, 1);
	redswitch(p);
	rc = prog_go(p, P_WAIT);
	redunswitch(p);
	rdstdin = redclose(rdstdin, 0);
	rdstdout = redclose(rdstdout, 1);
	psstdout = redclose(psstdout, 1);
	psstdin = redclose(psstdin, 0);
	if ((p = p->next) != 0) {
	    pxf = pif;
	    pif = pof;
	    pof = pxf;
	    psstdin = redopen(pif, O_RDONLY, 0);
	}
    }
    unlink(pif);
    free(pif);
    unlink(pof);
    free(pof);
    for (pn = p = p1; p; p = pn) {
	pn = p->next;
	if (p->line)
	    free(p->line);
	if (p->cmd)
	    free(p->cmd);
	if (p->arg)
	    free(p->arg);
	if (p->inf)
	    free(p->inf);
	if (p->outf)
	    free(p->outf);
	free(p);
    }
    if (rc == -2)
	return 127;
    return rc < 0 ? 0xFF00 : rc;
}

int exec_command(char *cmd, int wait_flag)
{
#if defined(__MINGW32__)
    return system(cmd);
#else
    PRO *p;
    char *pif;
    int rc = 0;
    int cmd_str;

    pif = cmd;
    while (*pif == ' ')
	pif++;

    cmd = pif;
    cmd_str = TRUE;

    while (*pif++) {
	if (*pif == '\r') {
	    *pif = '\0';
	    break;
	} else if (*pif == '\n') {
	    *pif = '\0';
	    break;
	} else if (cmd_str) {
	    if (*pif == '/')
		*pif = '\\';
	} else if (cmd_str) {
	    if (*pif == ' ')
		cmd_str = FALSE;
	}
    }
    p = pars(cmd);

    if (wait_flag)
	rc = prog_go(p, P_WAIT);
    else
	rc = prog_go(p, P_NOWAIT);

    return rc;
#endif
}

#ifdef TEST
void main()
{
    char line_buff[STR_MAX];

    while (gets(line_buff)) {
	printf("\nreturn %04X\n", xsystem(line_buff));
    }
}
#endif /* TEST */
