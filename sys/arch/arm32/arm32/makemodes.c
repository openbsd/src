/* $NetBSD: makemodes.c,v 1.3 1996/03/16 00:13:12 thorpej Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name of the group nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * makemodes.c
 *
 * VIDC20 mode configuration program.
 *
 * Builds a list of configured modes using mode specifiers to select
 * the VIDC timings from a monitor definition file
 *
 * Not yet perfect, this is just experimental.
 *
 * Created      : 17/05/95
 * Last updated : 17/05/95
 */

#include <stdio.h>

/*#define VERBOSE*/

#define MAX_MD 100	/* Support up to 100 modes from a file */

struct md {
	int	md_xres;
	int	md_yres;
	int	md_pixelrate;
	int	md_htimings[6];
	int	md_vtimings[6];
	int	md_syncpol;
	int	md_framerate;
} mds[MAX_MD];

int	md;	/* Number of modes defined in the mds array */


void makemode __P((FILE *, int, int, int, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char line[100];		/* Line read from mdf */
	char token[40];		/* token read from mdf line */
	char value[60];		/* value of token read form mdf line */
	int startmode;		/* Are we between startmode and endmode tokens */
	char monitor[40];	/* monitor name */
	int dpms;		/* DPMS state for monitor */
	FILE *mdf_fd;		/* mdf file descriptor */
	FILE *out_fd;		/* output file descriptor */
	int loop;		/* loop counter */
	int x, y, c, f;		/* mode specifier variables */
	int params;
	char *ptr;

/* Check the args */
	
	if (argc < 3) {
		fprintf(stderr, "Syntax: makemodes <mdf> <outfile> <modes>\n");
		exit(10);
	}

/* Open the monitor definition file */
	
	mdf_fd = fopen(argv[1], "r");
	if (!mdf_fd) {
		fprintf(stderr, "Cannot open monitor definition file\n");
		exit(10);
	}

/* Open the output file */

	out_fd = fopen(argv[2], "w");
	if (!out_fd) {
		fprintf(stderr, "Cannot open output file\n");
		exit(10);
	}

/* Initialise some variables */

	startmode = 0;
	md = 0;
	dpms = 0;
	monitor[0] = 0;

/* Loop for each line in the monitor definition file */

	do {

/* get a line */

		if (fgets(line, 99, mdf_fd) == NULL)
			break;

		ptr = line;

/* Skip any spaces or tabs */

		while (*ptr == ' ' || *ptr == '\t')
			++ptr;

/* Ignore comment or blank lines */

		if (*ptr == '#' || strlen(ptr) < 2)
			continue;

/* Do we have a startmode or endmode token ? */

		if (sscanf(ptr, "%s", token) == 1) {
			if (strcmp(token, "startmode") == 0) {
				if (md < MAX_MD)
					startmode = 1;
			}
			if (strcmp(token, "endmode") == 0) {
				startmode = 0;
				++md;
			}
		}

/* Do we have a token:value line ? */

		if (sscanf(ptr, "%[^:]:%[^\n]\n", token, value) == 2) {
			if (strcmp(token, "monitor_title") == 0)
				strcpy(monitor, value);
			if (strcmp(token, "file_format") == 0) {
				if (atoi(value) != 1) {
					fprintf(stderr, "Unrecognised file format\n");
					exit(10);
				}
			}
			if (strcmp(token, "DPMS_state") == 0)
				dpms = atoi(value);
			if (strcmp(token, "x_res") == 0 && startmode)
				mds[md].md_xres = atoi(value);
			if (strcmp(token, "y_res") == 0 && startmode)
				mds[md].md_yres = atoi(value);
			if (strcmp(token, "pixel_rate") == 0 && startmode)
				mds[md].md_pixelrate = atoi(value);
			if (strcmp(token, "h_timings") == 0 && startmode)
				sscanf(value, "%d,%d,%d,%d,%d,%d",
					&mds[md].md_htimings[0],
					&mds[md].md_htimings[1],
					&mds[md].md_htimings[2],
					&mds[md].md_htimings[3],
					&mds[md].md_htimings[4],
					&mds[md].md_htimings[5]);
			if (strcmp(token, "v_timings") == 0 && startmode)
				sscanf(value, "%d,%d,%d,%d,%d,%d",
					&mds[md].md_vtimings[0],
					&mds[md].md_vtimings[1],
					&mds[md].md_vtimings[2],
					&mds[md].md_vtimings[3],
					&mds[md].md_vtimings[4],
					&mds[md].md_vtimings[5]);
			if (strcmp(token, "sync_pol") == 0 && startmode)
				mds[md].md_syncpol = atoi(value);
		}
		
	}
	while (!feof(mdf_fd));

/* We have finished with the monitor definition file */
	
	fclose(mdf_fd);	

#ifdef VERBOSE

/* This was for debugging */

	for (loop = 0; loop < md; ++loop) {
		printf("%d x %d: %d %d [%d,%d,%d,%d,%d,%d] [%d,%d,%d,%d,%d,%d]\n",
			mds[loop].md_xres,
			mds[loop].md_yres,
			mds[loop].md_pixelrate,
			mds[loop].md_syncpol,
			mds[loop].md_htimings[0],
			mds[loop].md_htimings[1],
			mds[loop].md_htimings[2],
			mds[loop].md_htimings[3],
			mds[loop].md_htimings[4],
			mds[loop].md_htimings[5],
			mds[loop].md_vtimings[0],
			mds[loop].md_vtimings[1],
			mds[loop].md_vtimings[2],
			mds[loop].md_vtimings[3],
			mds[loop].md_vtimings[4],
			mds[loop].md_vtimings[5]);
	}
#endif

/* Start building the output file */

	fprintf(out_fd, "/*\n");
	fprintf(out_fd, " * MACHINE GENERATED: DO NOT EDIT\n");
	fprintf(out_fd, " *\n");
	fprintf(out_fd, " * %s, from %s\n", argv[2], argv[1]);
	fprintf(out_fd, " */\n\n");
	fprintf(out_fd, "#include <sys/types.h>\n");
	fprintf(out_fd, "#include <machine/vidc.h>\n\n");
	fprintf(out_fd, "char *monitor=\"%s\";\n", monitor);
	fprintf(out_fd, "int dpms=%d;\n", dpms);
	fprintf(out_fd, "\n", dpms);
	fprintf(out_fd, "struct vidc_mode vidcmodes[] = {\n");

	loop = 3;

/* Loop over the rest of the args processing then as mode specifiers */

/* NOTE: A mode specifier cannot have a space in it at the moment */
	
	while (argv[loop]) {
		printf("%s ==> ", argv[loop]);
		f = -1;
		c = 256;
		params = sscanf(argv[loop], "X%dY%dC%dF%d", &x, &y, &c, &f);
		if (params < 2)
			params = sscanf(argv[loop], "%d,%d,%d,%d", &x, &y, &c, &f);
		if (params == 2 || params == 4)
			makemode(out_fd, x, y, c, f);
		else if (params == 3)
			makemode(out_fd, x, y, 256, c);
		else
			printf("Invalid mode specifier\n");
		printf("\n");
		++loop;
	}

/* Finish off the output file */

	fprintf(out_fd, "  { 0 }\n");
	fprintf(out_fd, "};\n");
	
	fclose(out_fd);
}


/* Locate an appropriate mode for the specifier and write it to the file */

void makemode(out_fd, x, y, c, f)
	FILE *out_fd;
	int x, y, c, f;
{
	int loop;		/* loop counter */
	float framerate;	/* frame rate */
	int fr;			/* integer frame rate */
	int found = -1;		/* array index of found mode */
	int max = -1;		/* maximum frame rate found */
	int pos = -1;		/* array index of max frame rate */
 
/* Print some info */

	printf("%d x %d x %d x %d : ", x, y, c, f);

/* Scan the modes */

	for (loop = 0; loop < md; ++loop) {

/* X and Y have to match */

		if (mds[loop].md_xres == x && mds[loop].md_yres == y) {

/* Now calculate the frame rate */

			framerate = (float)mds[loop].md_pixelrate /
					(float)(mds[loop].md_htimings[0] +
					mds[loop].md_htimings[1] +
					mds[loop].md_htimings[2] +
					mds[loop].md_htimings[3] +
					mds[loop].md_htimings[4] +
					mds[loop].md_htimings[5]) /
					(float)(mds[loop].md_vtimings[0] +
					mds[loop].md_vtimings[1] +
					mds[loop].md_vtimings[2] +
					mds[loop].md_vtimings[3] +
					mds[loop].md_vtimings[4] +
					mds[loop].md_vtimings[5]);
			framerate = framerate * 1000;
			fr = (framerate + 0.5);
			mds[loop].md_framerate = fr;

/* Print it as info */

			printf("%d ", fr);

/* Is this a new maximum ? */

			if (max < fr) {
				max = fr;
				pos = loop;
			}			

/* Does it match the specified frame rate ? */

			if (fr == f)
				found = loop;
		}
	}

/* No exact match so use the max */

	if (found == -1)
		found = pos;

/* Do we have an entry for this X & Y resolution */
		
	if (found != -1) {
		fprintf(out_fd, "  { %d,/**/%d, %d, %d, %d, %d, %d,/**/%d, %d, %d, %d, %d, %d,/**/%d,/**/%d, %d },\n",
			mds[found].md_pixelrate,
			mds[found].md_htimings[0],
			mds[found].md_htimings[1],
			mds[found].md_htimings[2],
			mds[found].md_htimings[3],
			mds[found].md_htimings[4],
			mds[found].md_htimings[5],
			mds[found].md_vtimings[0],
			mds[found].md_vtimings[1],
			mds[found].md_vtimings[2],
			mds[found].md_vtimings[3],
			mds[found].md_vtimings[4],
			mds[found].md_vtimings[5],
			ffs(c),
			mds[found].md_syncpol,
			mds[found].md_framerate);
	}
	else {
		fprintf(stderr, "Cannot find mode\n");
	}
}

/* End of makemodes.c */
