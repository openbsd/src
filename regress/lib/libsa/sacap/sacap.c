#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libsa.h"

struct sa_par par;
struct sa_cap cap;

void
pr_enc(struct sa_enc *enc)
{
	fprintf(stderr, "%s%d", enc->sig ? "s" : "u", enc->bits);
	if (enc->bps > 1)
		fprintf(stderr, "%s", enc->le ? "le" : "be");
	if (enc->bps != SA_BPS(enc->bits))
		fprintf(stderr, "%d%s", enc->bps, enc->msb ? "msb" : "lsb");
}

void
cap_pr(struct sa_cap *cap)
{
	unsigned n, i;

	for (n = 0; n < cap->nconf; n++) {
		fprintf(stderr, "config %d\n", n);
		fprintf(stderr, "\tenc:");
		for (i = 0; i < SA_NENC; i++) {
			if (cap->confs[n].enc & (1 << i)) {
				fprintf(stderr, " ");
				pr_enc(&cap->enc[i]);
			}
		}
		fprintf(stderr, "\n\tpchan:");
		for (i = 0; i < SA_NCHAN; i++) {
			if (cap->confs[n].pchan & (1 << i))
				fprintf(stderr, " %d", cap->pchan[i]);
		}
		fprintf(stderr, "\n\trchan:");
		for (i = 0; i < SA_NCHAN; i++) {
			if (cap->confs[n].rchan & (1 << i))
				fprintf(stderr, " %d", cap->rchan[i]);
		}
		fprintf(stderr, "\n\trate:");
		for (i = 0; i < SA_NRATE; i++) {
			if (cap->confs[n].rate & (1 << i))
				fprintf(stderr, " %d", cap->rate[i]);
		}
		fprintf(stderr, "\n");
	}	
}

void
usage(void) {
	fprintf(stderr, "usage: sacap [-pr]\n");
}
 
int
main(int argc, char **argv) {
	int ch;
	unsigned mode = SA_PLAY | SA_REC;
	struct sa_hdl *hdl;
	
	while ((ch = getopt(argc, argv, "pr")) != -1) {
		switch(ch) {
		case 'p':
			mode &= ~SA_REC;
			break;
		case 'r':
			mode &= ~SA_PLAY;
			break;
		default:
			usage();
			exit(1);
			break;
		}
	}
	if (mode == 0) {
		fprintf(stderr, "-p and -r flags are mutualy exclusive\n");
		exit(1);
	}
	hdl = sa_open(NULL, mode, 0);
	if (hdl == NULL) {
		fprintf(stderr, "sa_open() failed\n");
		exit(1);
	}
	if (!sa_getcap(hdl, &cap)) {
		fprintf(stderr, "sa_setcap() failed\n");
		exit(1);
	}
	cap_pr(&cap);
	sa_close(hdl);
	return 0;
}
