/*	$OpenBSD: grfconfig.c,v 1.7 2002/09/06 22:45:06 deraadt Exp $	*/
/*	$NetBSD: grfconfig.c,v 1.6 1997/07/29 23:41:12 veego Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ezra Story and Bernd Ernesti.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#ifndef lint
static char rcsid[] = "$OpenBSD: grfconfig.c,v 1.7 2002/09/06 22:45:06 deraadt Exp $";
#endif /* not lint */

#include <sys/file.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <amiga/dev/grfioctl.h>

extern char *optarg;
extern int optind;

static void print_rawdata(struct grfvideo_mode *, int);

static struct grf_flag {
	u_short	grf_flag_number;
	char	*grf_flag_name;
} grf_flags[] = {
	{ GRF_FLAGS_DBLSCAN,		"doublescan" },
	{ GRF_FLAGS_LACE,		"interlace" },
	{ GRF_FLAGS_PHSYNC,		"+hsync" },
	{ GRF_FLAGS_NHSYNC,		"-hsync" },
	{ GRF_FLAGS_PVSYNC,		"+vsync" },
	{ GRF_FLAGS_NVSYNC,		"-vsync" },
	{ GRF_FLAGS_SYNC_ON_GREEN,	"sync-on-green" },
	{ 0,				0 }
};

/*
 * Dynamic mode loader for OpenBSD/Amiga grf devices.
 */
int
main(int argc, char *argv[])
{
	struct	grfvideo_mode gv[1];
	struct	grf_flag *grf_flagp;
	FILE	*fp;
	int	c, y, grffd;
	int	i, lineno = 0;
	int	uplim, lowlim;
	char	rawdata = 0, testmode = 0;
	char	*grfdevice = 0;
	char	*modefile = 0;
	char	buf[_POSIX2_LINE_MAX], obuf[_POSIX2_LINE_MAX];
	char	*cps[31];
	char	*p;
	char	*errortext;

	while ((c = getopt(argc, argv, "rt")) != -1) {
		switch (c) {
		case 'r':	/* raw output */
			rawdata = 1;
			break;
		case 't':	/* test the modefile without setting it */
			testmode = 1;
			break;
		default:
			printf("grfconfig [-r] device [file]\n");
			return (1);
		}
	}
	argc -= optind;
	argv += optind;


	if (argc >= 1)
		grfdevice = argv[0];
	else {
		printf("grfconfig: No grf device specified.\n");
		return (1);
	}

	if (argc >= 2)
		modefile = argv[1];

	if ((grffd = open(grfdevice, O_RDWR)) < 0) {
		printf("grfconfig: can't open grf device.\n");
		return (1);
	}
	/* If a mode file is specificied, load it in, don't display any info. */

	if (modefile) {
		if (!(fp = fopen(modefile, "r"))) {
			printf("grfconfig: Cannot open mode definition "
			    "file.\n");
			return (1);
		}
		while (fgets(buf, sizeof(buf), fp)) {
			/*
			 * check for end-of-section, comments, strip off trailing
			 * spaces and newline character.
			 */
			for (p = buf; isspace(*p); ++p)
				continue;
			if (*p == '\0' || *p == '#')
				continue;
			for (p = strchr(buf, '\0'); isspace(*--p);)
				continue;
			*++p = '\0';

			snprintf(obuf, sizeof obuf, "%s", buf);
			lineno = lineno + 1;

			for (i = 0, *cps = strtok(buf, " \b\t\r\n");
			    cps[i] != NULL && i < 30; i++)
				cps[i + 1] = strtok(NULL, " \b\t\r\n");
			cps[i] = NULL;

			if (cps[13] == NULL) {
				printf("grfconfig: too few values in mode "
				    "definition file:\n %s\n", obuf);
				return (1);
			}

			gv->pixel_clock	= atoi(cps[1]);
			gv->disp_width	= atoi(cps[2]);
			gv->disp_height	= atoi(cps[3]);
			gv->depth	= atoi(cps[4]);
			gv->hblank_start	= atoi(cps[5]);
			gv->hsync_start	= atoi(cps[6]);
			gv->hsync_stop	= atoi(cps[7]);
			gv->htotal	= atoi(cps[8]);
			gv->vblank_start	= atoi(cps[9]);
			gv->vsync_start	= atoi(cps[10]);
			gv->vsync_stop	= atoi(cps[11]);
			gv->vtotal	= atoi(cps[12]);

			if ((y = atoi(cps[0])))
				gv->mode_num = y;
			else
				if (strncasecmp("c", cps[0], 1) == 0) {
					gv->mode_num = 255;
					gv->depth = 4;
				} else {
					printf("grfconfig: Illegal mode "
					    "number: %s\n", cps[0]);
					return (1);
				}

			if ((gv->pixel_clock == 0) ||
			    (gv->disp_width == 0) ||
			    (gv->disp_height == 0) ||
			    (gv->depth == 0) ||
			    (gv->hblank_start == 0) ||
			    (gv->hsync_start == 0) ||
			    (gv->hsync_stop == 0) ||
			    (gv->htotal == 0) ||
			    (gv->vblank_start == 0) ||
			    (gv->vsync_start == 0) ||
			    (gv->vsync_stop == 0) ||
			    (gv->vtotal == 0)) {
				printf("grfconfig: Illegal value in "
				    "mode #%d:\n %s\n", gv->mode_num, obuf);
				return (1);
			}

			if (strstr(obuf, "default") != NULL) {
				gv->disp_flags = GRF_FLAGS_DEFAULT;
			} else {
				gv->disp_flags = GRF_FLAGS_DEFAULT;
				for (grf_flagp = grf_flags;
				  grf_flagp->grf_flag_number; grf_flagp++) {
				    if (strstr(obuf, grf_flagp->grf_flag_name) != NULL) {
					gv->disp_flags |= grf_flagp->grf_flag_number;
				    }
				}
				if (gv->disp_flags == GRF_FLAGS_DEFAULT) {
					printf("grfconfig: Your are using an "
					    "mode file with an obsolete "
					    "format.\n See the manpage of "
					    "grfconfig for more information "
					    "about the new mode definition "
					    "file.\n");
					return (1);
				}
			}

			/*
			 * Check for impossible gv->disp_flags:
			 * doublescan and interlace,
			 * +hsync and -hsync
			 * +vsync and -vsync.
			 */
			errortext = NULL;
			if ((gv->disp_flags & GRF_FLAGS_DBLSCAN) &&
			    (gv->disp_flags & GRF_FLAGS_LACE))
				errortext = "Interlace and Doublescan";
			if ((gv->disp_flags & GRF_FLAGS_PHSYNC) &&
			    (gv->disp_flags & GRF_FLAGS_NHSYNC))
				errortext = "+hsync and -hsync";
			if ((gv->disp_flags & GRF_FLAGS_PVSYNC) &&
			    (gv->disp_flags & GRF_FLAGS_NVSYNC))
				errortext = "+vsync and -vsync";

			if (errortext != NULL) {
				printf("grfconfig: Illegal flags in "
				    "mode #%d: %s are both defined!\n",
				    gv->mode_num, errortext);
				return (1);
			}

			/* Check for old horizontal cycle values */
			if ((gv->htotal < (gv->disp_width / 4))) {
				gv->hblank_start *= 8;
				gv->hsync_start *= 8;
				gv->hsync_stop *= 8;
				gv->htotal *= 8;
				printf("grfconfig: Old and no longer "
				    "supported horizontal videoclock cycle "
				    "values.\n Wrong mode line:\n  %s\n "
				    "This could be a possible good mode "
				    "line:\n  ", obuf);
				printf("%d ", gv->mode_num);
				print_rawdata(gv, 0);
				printf(" See the manpage of grfconfig for "
				    "more information about the new mode "
				    "definition file.\n");
				return (1);
			}

			/* Check for old interlace or doublescan modes */
			uplim = gv->disp_height + (gv->disp_height / 4);
			lowlim = gv->disp_height - (gv->disp_height / 4);
			if (((gv->vtotal * 2) > lowlim) &&
			    ((gv->vtotal * 2) < uplim)) {
				gv->vblank_start *= 2;
				gv->vsync_start *= 2;
				gv->vsync_stop *= 2;
				gv->vtotal *= 2;
				gv->disp_flags &= ~GRF_FLAGS_DBLSCAN;
				gv->disp_flags |= GRF_FLAGS_LACE;
				printf("grfconfig: Old and no longer "
				    "supported vertical values for "
				    "interlace modes.\n Wrong mode "
				    "line:\n  %s\n This could be a "
				    "possible good mode line:\n  ", obuf);
				printf("%d ", gv->mode_num);
				print_rawdata(gv, 0);
				printf(" See the manpage of grfconfig for "
				    "more information about the new mode "
				    "definition file.\n");
				return (1);
			} else if (((gv->vtotal / 2) > lowlim) &&
			    ((gv->vtotal / 2) < uplim)) {
				gv->vblank_start /= 2;
				gv->vsync_start /= 2;
				gv->vsync_stop /= 2;
				gv->vtotal /= 2;
				gv->disp_flags &= ~GRF_FLAGS_LACE;
				gv->disp_flags |= GRF_FLAGS_DBLSCAN;
				printf("grfconfig: Old and no longer "
				    "supported vertical values for "
				    "doublescan modes.\n Wrong mode "
				    "line:\n  %s\n This could be a "
				    "possible good mode line:\n  ", obuf);
				printf("%d ", gv->mode_num);
				print_rawdata(gv, 0);
				printf(" See the manpage of grfconfig for "
				    "more information about the new mode "
				    "definition file.\n");
				return (1);
			}

			if (testmode == 1) {
				if (lineno == 1)
					printf("num clk wid hi dep hbs "
					    "hss hse ht vbs vss vse vt "
					    "flags\n");
				printf("%d ", gv->mode_num);
				print_rawdata(gv, 1);
			} else {
				gv->mode_descr[0] = 0;
				if (ioctl(grffd, GRFIOCSETMON, (char *) gv) < 0)
					printf("grfconfig: bad monitor "
					    "definition for mode #%d.\n",
					    gv->mode_num);
			}
		}
		fclose(fp);
	} else {
		ioctl(grffd, GRFGETNUMVM, &y);
		y += 2;
		for (c = 1; c < y; c++) {
			c = gv->mode_num = (c != (y - 1)) ? c : 255;
			if (ioctl(grffd, GRFGETVMODE, gv) < 0)
				continue;
			if (rawdata) {
				if (c == 255)
					printf("c ");
				else
					printf("%d ", c);
				print_rawdata(gv, 0);
				continue;
			}
			if (c == 255)
				printf("Console: ");
			else
				printf("%2d: ", gv->mode_num);

			printf("%dx%d",
			    gv->disp_width,
			    gv->disp_height);

			if (c != 255)
				printf("x%d", gv->depth);
			else
				printf(" (%dx%d)",
				    gv->disp_width / 8,
				    gv->disp_height / gv->depth);

			printf("\t%ld.%ldkHz @ %ldHz",
			    gv->pixel_clock / (gv->htotal * 1000),
			    (gv->pixel_clock / (gv->htotal * 100)) % 10,
			    gv->pixel_clock / (gv->htotal * gv->vtotal));
			printf(" flags:");
				
			if (gv->disp_flags == GRF_FLAGS_DEFAULT) {
				printf(" default");
			} else {
				for (grf_flagp = grf_flags;
				    grf_flagp->grf_flag_number; grf_flagp++) {
					if (gv->disp_flags &
					    grf_flagp->grf_flag_number) {
						printf(" %s",
						    grf_flagp->grf_flag_name);
					}
				}
			}
			printf("\n");
		}
	}

	close(grffd);
	return (0);
}

static void
print_rawdata(struct grfvideo_mode *gv, int rawflags)
{
	struct	grf_flag *grf_flagp;

	printf("%ld %d %d %d %d %d %d %d %d %d %d %d",
	    gv->pixel_clock, gv->disp_width,
	    gv->disp_height, gv->depth,
	    gv->hblank_start, gv->hsync_start,
	    gv->hsync_stop, gv->htotal,
	    gv->vblank_start, gv->vsync_start,
	    gv->vsync_stop, gv->vtotal);

	if (rawflags) {
		printf(" 0x%.2x", gv->disp_flags);
	} else {
		if (gv->disp_flags == GRF_FLAGS_DEFAULT) {
			printf(" default");
		} else {
			for (grf_flagp = grf_flags;
			    grf_flagp->grf_flag_number; grf_flagp++) {
				if (gv->disp_flags &
				    grf_flagp->grf_flag_number) {
					printf(" %s",
					    grf_flagp->grf_flag_name);
				}
			}
		}
	}
	printf("\n");
}
