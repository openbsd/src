/*	$NetBSD: emuspeed.c,v 1.3 1998/06/15 14:43:25 is Exp $	*/

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "speed.h"

#define PRECISION 500

const struct test {
	char *name; 
	void (*func)__P((int));
	char *comment;
} testlist[] = {
	{"Illegal", illegal, "(test: unimplemented)"},
	{"mulsl Da,Db", mul32sreg, "(test: should be native)"},
	{"mulsl sp@(8),Da", mul32smem, "(test: should be native)\n"},
	
	{"mulsl Dn,Da:Db", mul64sreg, "emulated on 68060"},
	{"mulul Dn,Da:Db", mul64ureg, "\t\""},
	{"mulsl sp@(8),Da:Db", mul64smem, "\t\""},
	{"mulul sp@(8),Da:Db", mul64umem, "\t\"\n"},

	{"divsl Da:Db,Dn", div64sreg, "\t\""},
	{"divul Da:Db,Dn", div64ureg, "\t\""},
	{"divsl Da:Db,sp@(8)", div64smem, "\t\""},
	{"divul Da:Db,sp@(8)", div64umem, "\t\"\n"},

	{NULL, NULL, NULL}
};

jmp_buf jbuf;
void illhand (int);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	const struct test *t;
	clock_t start, stop;
	int count;


	if (signal(SIGILL, &illhand))
		warn("%s: can't install illegal instruction handler.",
		    argv[0]);

	printf("Speed of instructions which are emulated on some cpus:\n\n");
	(void)sleep(1);
	for (t=testlist; t->name; t++) {
		printf("%-20s", t->name);
		fflush(stdout);

		if (setjmp(jbuf)) {
			printf("%15s    %s\n", "[unimplemented]", t->comment);
			continue;
		}
			
		count = 1000;
		do {
			count *= 2;
			start = clock();
			t->func(count);
			stop = clock();
		} while ((stop - start) < PRECISION);
		printf("%13d/s    %s\n",
		    CLOCKS_PER_SEC*(count /(stop - start)),
		    t->comment);
	}
	exit (0);
}

void
illhand(int i)
{
	longjmp(jbuf, 1);
}
