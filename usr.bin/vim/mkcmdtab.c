/*	$OpenBSD: mkcmdtab.c,v 1.1.1.1 1996/09/07 21:40:24 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * mkcmdtab.c: separate program that reads cmdtab.tab and produces cmdtab.h
 *
 *	call with: mkcmdtab cmdtab.tab cmdtab.h
 */

#include "vim.h"

#if defined(UTS4)
    int
#else
	void
#endif
main(argc, argv)
	int		argc;
	char	**argv;
{
	register int	c;
	char			buffer[100];
	int				count;
	int				i;
	FILE			*ifp, *ofp;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: mkcmdtab cmdtab.tab cmdtab.h\n");
		exit(10);
	}
	ifp = fopen(argv[1], "r");
	if (ifp == NULL)
	{
		perror(argv[1]);
		exit(10);
	}
	ofp = fopen(argv[2], "w");
	if (ofp == NULL)
	{
		perror(argv[2]);
		exit(10);
	}

	while ((c = getc(ifp)) != '|' && c != EOF)
		putc(c, ofp);
	fprintf(ofp, "THIS FILE IS AUTOMATICALLY PRODUCED - DO NOT EDIT");
	while ((c = getc(ifp)) != '|' && c != EOF)
		;
	while ((c = getc(ifp)) != '|' && c != EOF)
		putc(c, ofp);

	count = 0;
	while ((c = getc(ifp)) != '|' && c != EOF)
	{
		putc(c, ofp);
		while ((c = getc(ifp)) != '"' && c != EOF)
			putc(c, ofp);
		putc(c, ofp);

		i = 0;
		while ((c = getc(ifp)) != '"' && c != EOF)
		{
			putc(c, ofp);
			buffer[i++] = c;
		}
		putc(c, ofp);
		buffer[i] = 0;

		while ((c = getc(ifp)) != '\n' && c != EOF)
			putc(c, ofp);
		putc(c, ofp);

		switch (buffer[0])
		{
			case '@':	strcpy(buffer, "at");
						break;
			case '!':	strcpy(buffer, "bang");
						break;
			case '<':	strcpy(buffer, "lshift");
						break;
			case '>':	strcpy(buffer, "rshift");
						break;
			case '=':	strcpy(buffer, "equal");
						break;
			case '&':	strcpy(buffer, "and");
						break;
			case '~':	strcpy(buffer, "tilde");
						break;
			case '#':	strcpy(buffer, "pound");
						break;
		}
					
		fprintf(ofp, "#define CMD_%s %d\n", buffer, count++);
	}

	fprintf(ofp, "#define CMD_SIZE %d\n", count);

	while ((c = getc(ifp)) != '|' && c != EOF)
		putc(c, ofp);

	if (c != '|')
	{
		fprintf(stderr, "not enough |'s\n");
		exit(1);
	}
	exit(0);
}
