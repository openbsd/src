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

static const char *talk[] = {
	"Write more code.",
	"Make more commits.",
	"That's because you have been slacking.",
	"slacker!",
	"That's what happens when you're lazy.",
	"idler!",
	"slackass!",
	"lazy bum!",
	"Stop slacking you lazy bum!",
	"slacker slacker lazy bum bum bum slacker!",
	"I could search... but I'm a lazy bum ;)",
	"sshutup sshithead, ssharpsshooting susshi sshplats ssharking assholes.",
	"Lazy bums slacking on your asses.",
	"35 commits an hour? That's pathetic!",
	"Fine software takes time to prepare.  Give a little slack.",
	"Just a minute ago we were hugging and now you, guys, do not love me anymore",
	"I'll let you know when I need to floss my teeth",
	"If you can't figure out yourself, you're lacking some mental faculties",
	"I am just stating a fact",
	"blah blah",
	"i'd love to hack, but i can't",
	"Wait, yes, I am on drugs",
	"during release it is a constant.  almost noone helps.",
	"i let you guys do whatever you wanted",
	"you bring new meaning to the terms slackass. I will have to invent a new term.",
	"if they cut you out, muddy their back yards",
	"Make them want to start over, and play nice the next time.",
	"It is clear that this has not been thought through."
};

static const int ntalk = sizeof(talk)/sizeof(talk[0]);

static int
theo_analyze(int f, int n)
{
	const char *str;
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
