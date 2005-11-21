/*	$OpenBSD: lk201.c,v 1.6 2005/11/21 18:16:37 millert Exp $	*/

/*
 * The LK201 keycode mapping routine is here, along with initialization
 * functions for the keyboard and mouse.
 */


#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/selinfo.h>
#include <dev/cons.h>

#include <pmax/include/pmioctl.h>

#include <pmax/dev/lk201.h>


/* Exported functions */
extern int kbdMapChar(int keycode);

extern void KBDReset(dev_t dev, void (*putc)(dev_t, int));

/*
 * Keyboard to Ascii, unshifted.
 */
static unsigned char unshiftedAscii[] = {
/*  0 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/*  4 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/*  8 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/*  c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 10 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 14 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 18 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 1c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 20 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 24 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 28 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 2c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 30 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 34 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 38 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 3c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 40 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 44 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 48 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 4c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 50 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 54 */ KBD_NOKEY,	KBD_NOKEY,	KBD_F1,		KBD_F2,
/* 58 */ KBD_F3,	KBD_F4,		KBD_F5,		KBD_NOKEY,
/* 5c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 60 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 64 */ KBD_F6,	KBD_F7,		KBD_F8,		KBD_F9,
/* 68 */ KBD_F10,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 6c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 70 */ KBD_NOKEY,	'\033',		KBD_F12,	KBD_F13,
/* 74 */ KBD_F14,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 78 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 7c */ KBD_HELP,	KBD_DO,		KBD_NOKEY,	KBD_NOKEY,
/* 80 */ KBD_F17,	KBD_F18,	KBD_F19,	KBD_F20,
/* 84 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 88 */ KBD_NOKEY,	KBD_NOKEY,	KBD_FIND,	KBD_INSERT,
/* 8c */ KBD_REMOVE,	KBD_SELECT,	KBD_PREVIOUS,	KBD_NEXT,
/* 90 */ KBD_NOKEY,	KBD_NOKEY,	'0',		KBD_NOKEY,
/* 94 */ '.',		KBD_KP_ENTER,	'1',		'2',
/* 98 */ '3',		'4',		'5',		'6',
/* 9c */ ',',		'7',		'8',		'9',
/* a0 */ '-',		KBD_KP_F1,	KBD_KP_F2,	KBD_KP_F3,
/* a4 */ KBD_KP_F4,	KBD_NOKEY,	KBD_NOKEY,	KBD_LEFT,
/* a8 */ KBD_RIGHT,	KBD_DOWN,	KBD_UP,		KBD_NOKEY,
/* ac */ KBD_NOKEY,	KBD_NOKEY,	KBD_SHIFT,	KBD_CONTROL,
/* b0 */ KBD_CAPSLOCK,	KBD_ALTERNATE,	KBD_NOKEY,	KBD_NOKEY,
/* b4 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* b8 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* bc */ KBD_DEL,	KBD_RET,	KBD_TAB,	'`',
/* c0 */ '1',		'q',		'a',		'z',
/* c4 */ KBD_NOKEY,	'2',		'w',		's',
/* c8 */ 'x',		'<',		KBD_NOKEY,	'3',
/* cc */ 'e',		'd',		'c',		KBD_NOKEY,
/* d0 */ '4',		'r',		'f',		'v',
/* d4 */ ' ',		KBD_NOKEY,	'5',		't',
/* d8 */ 'g',		'b',		KBD_NOKEY,	'6',
/* dc */ 'y',		'h',		'n',		KBD_NOKEY,
/* e0 */ '7',		'u',		'j',		'm',
/* e4 */ KBD_NOKEY,	'8',		'i',		'k',
/* e8 */ ',',		KBD_NOKEY,	'9',		'o',
/* ec */ 'l',		'.',		KBD_NOKEY,	'0',
/* f0 */ 'p',		KBD_NOKEY,	';',		'/',
/* f4 */ KBD_NOKEY,	'=',		']',		'\\',
/* f8 */ KBD_NOKEY,	'-',		'[',		'\'',
/* fc */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
};

/*
 * Keyboard to Ascii, shifted.
 */
static unsigned char shiftedAscii[] = {
/*  0 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/*  4 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/*  8 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/*  c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 10 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 14 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 18 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 1c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 20 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 24 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 28 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 2c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 30 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 34 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 38 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 3c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 40 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 44 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 48 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 4c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 50 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 54 */ KBD_NOKEY,	KBD_NOKEY,	KBD_F1,		KBD_F2,
/* 58 */ KBD_F3,	KBD_F4,		KBD_F5,		KBD_NOKEY,
/* 5c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 60 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 64 */ KBD_F6,	KBD_F7,		KBD_F8,		KBD_F9,
/* 68 */ KBD_F10,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 6c */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 70 */ KBD_NOKEY,	KBD_F11,	KBD_F12,	KBD_F13,
/* 74 */ KBD_F14,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 78 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 7c */ KBD_HELP,	KBD_DO,		KBD_NOKEY,	KBD_NOKEY,
/* 80 */ KBD_F17,	KBD_F18,	KBD_F19,	KBD_F20,
/* 84 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* 88 */ KBD_NOKEY,	KBD_NOKEY,	KBD_FIND,	KBD_INSERT,
/* 8c */ KBD_REMOVE,	KBD_SELECT,	KBD_PREVIOUS,	KBD_NEXT,
/* 90 */ KBD_NOKEY,	KBD_NOKEY,	'0',		KBD_NOKEY,
/* 94 */ '.',		KBD_KP_ENTER,	'1',		'2',
/* 98 */ '3',		'4',		'5',		'6',
/* 9c */ ',',		'7',		'8',		'9',
/* a0 */ '-',		KBD_KP_F1,	KBD_KP_F2,	KBD_KP_F3,
/* a4 */ KBD_KP_F4,	KBD_NOKEY,	KBD_NOKEY,	KBD_LEFT,
/* a8 */ KBD_RIGHT,	KBD_DOWN,	KBD_UP,		KBD_NOKEY,
/* ac */ KBD_NOKEY,	KBD_NOKEY,	KBD_SHIFT,	KBD_CONTROL,
/* b0 */ KBD_CAPSLOCK,	KBD_ALTERNATE,	KBD_NOKEY,	KBD_NOKEY,
/* b4 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* b8 */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
/* bc */ KBD_DEL,	KBD_RET,	KBD_TAB,	'~',
/* c0 */ '!',		'q',		'a',		'z',
/* c4 */ KBD_NOKEY,	'@',		'w',		's',
/* c8 */ 'x',		'>',		KBD_NOKEY,	'#',
/* cc */ 'e',		'd',		'c',		KBD_NOKEY,
/* d0 */ '$',		'r',		'f',		'v',
/* d4 */ ' ',		KBD_NOKEY,	'%',		't',
/* d8 */ 'g',		'b',		KBD_NOKEY,	'^',
/* dc */ 'y',		'h',		'n',		KBD_NOKEY,
/* e0 */ '&',		'u',		'j',		'm',
/* e4 */ KBD_NOKEY,	'*',		'i',		'k',
/* e8 */ '<',		KBD_NOKEY,	'(',		'o',
/* ec */ 'l',		'>',		KBD_NOKEY,	')',
/* f0 */ 'p',		KBD_NOKEY,	':',		'?',
/* f4 */ KBD_NOKEY,	'+',		'}',		'|',
/* f8 */ KBD_NOKEY,	'_',		'{',		'"',
/* fc */ KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,	KBD_NOKEY,
};

/* 
 * Keyboard initialization string.
 */
static u_char kbdInitString[] = {
	LK_LED_ENABLE, LED_ALL,		/* show we are resetting keyboard */
	LK_DEFAULTS,
	LK_CMD_MODE(LK_AUTODOWN, 1), 
	LK_CMD_MODE(LK_AUTODOWN, 2), 
	LK_CMD_MODE(LK_AUTODOWN, 3), 
	LK_CMD_MODE(LK_DOWN, 4),	/* could also be LK_AUTODOWN */
	LK_CMD_MODE(LK_UPDOWN, 5),   
	LK_CMD_MODE(LK_UPDOWN, 6),   
	LK_CMD_MODE(LK_AUTODOWN, 7), 
	LK_CMD_MODE(LK_AUTODOWN, 8), 
	LK_CMD_MODE(LK_AUTODOWN, 9), 
	LK_CMD_MODE(LK_AUTODOWN, 10), 
	LK_CMD_MODE(LK_AUTODOWN, 11), 
	LK_CMD_MODE(LK_AUTODOWN, 12), 
	LK_CMD_MODE(LK_DOWN, 13), 
	LK_CMD_MODE(LK_AUTODOWN, 14),
	LK_AR_ENABLE,			/* we want autorepeat by default */
	LK_CL_ENABLE, 0x83,		/* keyclick, volume */
	LK_KBD_ENABLE,			/* the keyboard itself */
	LK_BELL_ENABLE, 0x83,		/* keyboard bell, volume */
	LK_LED_DISABLE, LED_ALL,	/* clear keyboard leds */
};


/*
 * Initialize the Keyboard.
 */
void
KBDReset(kbddev, putc)
	dev_t kbddev;
	void (*putc)(dev_t, int);
{
	register int i;
	static int inKBDReset;

	if (inKBDReset)
		return;
	inKBDReset = 1;
	for (i = 0; i < sizeof(kbdInitString); i++)
		(*putc)(kbddev, (int)kbdInitString[i]);
	inKBDReset = 0;
}

/*
 * ----------------------------------------------------------------------------
 *
 * kbdMapChar --
 *
 *	Map characters from the keyboard to ASCII. Return -1 if there is
 *	no valid mapping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Remember state of shift and control keys.
 *
 * ----------------------------------------------------------------------------
 */
int
kbdMapChar(cc)
	int cc;
{
	static u_char shiftDown;
	static u_char ctrlDown;
	static u_char lastChar;

	switch (cc) {
	case KEY_REPEAT:
		cc = lastChar;
		goto done;

	case KEY_UP:
		shiftDown = 0;
		ctrlDown = 0;
		return (-1);

	case KEY_SHIFT:
	case KEY_R_SHIFT:
		if (ctrlDown || shiftDown)
			shiftDown = 0;
		else
			shiftDown = 1;
		return (-1);

	case KEY_CONTROL:
		if (shiftDown || ctrlDown)
			ctrlDown = 0;
		else
			ctrlDown = 1;
		return (-1);

	case LK_POWER_ERROR:
	case LK_KDOWN_ERROR:
	case LK_INPUT_ERROR:
	case LK_OUTPUT_ERROR:
		log(LOG_WARNING,
			"lk201: keyboard error, code=%x\n", cc);
		return (-1);
	}
	if (shiftDown)
		cc = shiftedAscii[cc];
	else
		cc = unshiftedAscii[cc];
	if (cc >= KBD_NOKEY) {
		/*
		 * A function key was typed - ignore it.
		 */
		return (-1);
	}
	if (cc >= 'a' && cc <= 'z') {
		if (ctrlDown)
			cc = cc - 'a' + '\1'; /* ^A */
		else if (shiftDown)
			cc = cc - 'a' + 'A';
	} else if (ctrlDown) {
		if (cc >= '[' && cc <= '_')
			cc = cc - '@';
		else if (cc == ' ' || cc == '@')
			cc = '\0';
	}
	lastChar = cc;
done:
	return (cc);
}


static int (*raw_kbd_getc)(dev_t dev) = NULL;
static dev_t lk_in_dev = NODEV;

/*
 * Divert input from a serial port to the lk-201 keyboard handler.
 */
void
lk_divert(getfn, in_dev)
	int (*getfn)(dev_t dev) ;
	dev_t in_dev;
{
	raw_kbd_getc = getfn;
	lk_in_dev = in_dev;
}

/*
 * Get an ASCII character off of the keyboard.
 * Simply pass the getc request onto the underlying
 * serial driver, and map the resulting LK-201 keycode to ASCII.
 * FIXME: this design can't handle cursor or keypad keys,
 * and should be thrown away and replaced with a stackable
 * "Bstreams"-style driver.
 */
int
LKgetc(dev)
	dev_t dev;	/* ignored */
{
	register int c;

	extern	sccGetc();

#if 0
/*XXX*/ printf("LK-201 getc 0x%x( [%d %d]) in_dev [%d %d]\n",
	       raw_kbd_getc,
	       major(dev), minor(dev),
	       major(lk_in_dev), minor(lk_in_dev));
#endif

	if (raw_kbd_getc == NULL) {
		panic("Reading from LK-201 before keyboard driver diverted");
		return (-1);
	}

	for (;;) {
		/* c = (*cn_tab.cn_kbdgetc)(cn_tab.cn_dev); */
		c = (*raw_kbd_getc) (lk_in_dev);
#if 0
/*XXX*/ printf(" 0x%x [%c]", c, c);
#endif
		if (c == 0)
			return (-1);
		if ((c = kbdMapChar(c & 0xff)) >= 0)
			break;
	}
	return (c);
}


/*
 * Initialize the mouse.  (Doesn't really belong here.)
 */
void
MouseInit(mdev, putc, getc)
	dev_t mdev;
	void (*putc)(dev_t, int);
	int (*getc)(dev_t);
{
	int id_byte1, id_byte2, id_byte3, id_byte4;

	/*
	 * Initialize the mouse.
	 */
	(*putc)(mdev, MOUSE_SELF_TEST);
	id_byte1 = (*getc)(mdev);
	if (id_byte1 < 0) {
		printf("MouseInit: Timeout on 1st byte of self-test report\n");
		return;
	}
	id_byte2 = (*getc)(mdev);
	if (id_byte2 < 0) {
		printf("MouseInit: Timeout on 2nd byte of self-test report\n");
		return;
	}
	id_byte3 = (*getc)(mdev);
	if (id_byte3 < 0) {
		printf("MouseInit: Timeout on 3rd byte of self-test report\n");
		return;
	}
	id_byte4 = (*getc)(mdev);
	if (id_byte4 < 0) {
		printf("MouseInit: Timeout on 4th byte of self-test report\n");
		return;
	}
	if ((id_byte2 & 0x0f) != 0x2)
		printf("MouseInit: We don't have a mouse!!!\n");
	/*
	 * For some reason, the mouse doesn't see this command if it comes
	 * too soon after a self test.
	 */
	DELAY(100);
	(*putc)(mdev, MOUSE_INCREMENTAL);
}
