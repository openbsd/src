#include "def.h"
#include "kbd.h"
#include "funmap.h"

void theo_init(void);
static int	theo_analyze(int, int);
static int	theo(int, int);

static PF theo_pf[] = {
	theo_analyze,
};

static struct KEYMAPE (1 + IMAPEXT) theomap = {
	1,
	1 + IMAPEXT,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), theo_pf, NULL },
	}
};

static BUFFER *tbuf;

void
theo_init(void)
{
	funmap_add(theo, "theo");
	maps_add((KEYMAP *)&theomap, "theo");
}

static int
theo(int f, int n)
{
	BUFFER *bp;
	MGWIN *wp;

	bp = bfind("theo", TRUE);
	if (bclear(bp) != TRUE)
		return FALSE;

	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("theo");
	bp->b_nmodes = 1;

	if ((wp = popbuf(bp)) == NULL)
		return FALSE;

	tbuf = curbp = bp;
	curwp = wp;
	return TRUE;
}

static char *talk[] = {
	"Write more code.",
	"Make more commits.",
	"That's because you have been slacking.",
	"slacker!",
	"That's what happens when you're lazy.",
	"idler!",
	"slackass!",
	"lazy bum!",
	"Stop slacking you lazy bum!",
	"slacker slacker lazy bum bum bum slacker!"
};

static int ntalk = sizeof(talk)/sizeof(talk[0]);

static int
theo_analyze(int f, int n)
{
	char *str;
	int len;

	str = talk[arc4random() % ntalk];
	len = strlen(str);

	newline(FFRAND, 2);

	while (len--) {
		linsert(1, *str++);
	}

	newline(FFRAND, 2);

	return TRUE;
}
