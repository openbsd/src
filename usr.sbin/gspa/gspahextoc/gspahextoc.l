/*
 * Copyright (c) 1995 Ignatios Souvatzis
 * All rights reserved.
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

	int bufptr;
	unsigned short buf[4096];

	unsigned long binads;


	void dumpstart __P((char *address));
	void dumpbuf __P((void));
	void initbuf __P((char *address));
        void checkbuf __P((void));
%%
:[0-9A-Za-z]+	{if (bufptr>0) dumpbuf(); dumpstart(yytext+1);}
@[0-9A-Za-z]+	{if (bufptr>0) dumpbuf(); initbuf(yytext+1);}
[0-9A-Za-z]+	{checkbuf(); buf[bufptr++] = strtoul(yytext, 0, 16);}
[ \t\n]		;
%%

void
dumpbuf()
{
	int i;

	printf("\n\n\t%d, 0x%04x, 0x%04x, /* new block */", 
	    bufptr, binads >> 16, binads & 0xffff);
	for (i=0; i<bufptr; ++i) {
		if (i%8 == 0)
			printf("\n\t");
		printf("0x%04x, ", buf[i]);
	}
	binads += bufptr;
	bufptr = 0;
}

void
checkbuf()
{
	if (bufptr > (sizeof(buf)/sizeof(*buf)))
		dumpbuf();
}

void
initbuf(address)
	char *address;
{
	binads = strtoul(address, 0, 16);
	bufptr = 0;
}

void
dumpstart(address)
	char *address;
{
	u_int32_t startaddress;

	startaddress = strtoul(address, 0, 16);

	printf("\n\n\t2, 0xFFFF, 0xFEE0, 0x%04x, 0x%04x,\t/* start address */",
	    startaddress & 0xFFFF, startaddress >> 16);

	printf("\n\t2, 0xFFFF, 0xFFE0, 0x%04x, 0x%04x,",
	    startaddress & 0xFFFF, startaddress >> 16);
		
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	++argv; --argc;
	if (argc > 0) {
		yyin = fopen(argv[0], "r");
		printf("/*\n"
		    " * This file was automatically created from %s,\n"\
		    " * a TMS34010 assembler output file.\n", argv[0]);
	} else {
		yyin = stdin;
		printf("/*\n"
		    " * This file was automatically created from\n"\
		    " * a TMS34010 assembler output file.\n");
	}

	printf(" * Do not edit manually..\n");
	printf(" */\n");
	printf("#include <sys/types.h>\n");
	printf("u_int16_t tmscode[] = {\n\t");
	yylex();
	if (bufptr>0)
		 dumpbuf();
	printf("\n\t0\n};\n");
	
	exit(0);
}
