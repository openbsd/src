/* Our silly token storage data structure */

struct token {
	/* FIXME: Is 128 bytes always enough? */
	char tbuf[128];
	int32_t size;
        char *cell;
};


/* Function prototypes for unlog.c */

char *gettoken(int tnum, struct token *tok);

void restoretoken(struct token *tok);


/* Maximum number of tokens to save */

#define MAX_TOKENS 32

