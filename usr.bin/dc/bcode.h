#include <sys/types.h>
#include <ssl/bn.h>


struct number {
	BIGNUM	*number;
	u_int	scale;
};

enum stacktype {
	BCODE_NONE,
	BCODE_NUMBER,
	BCODE_STRING
};

enum bcode_compare {
	BCODE_EQUAL,
	BCODE_NOT_EQUAL,
	BCODE_LESS,
	BCODE_NOT_LESS,
	BCODE_GREATER,
	BCODE_NOT_GREATER
};

struct array;

struct value {
	union {
		struct number	*num;
		char		*string;
	} u;
	struct array	*array;
	enum stacktype	type;
};

struct array {
	struct value	*data;
	size_t		size;
};

struct stack {
	struct value	*stack;
	int		sp;
	int		size;
};

struct source;

struct vtable {
	int	(*readchar)(struct source *);
	int	(*unreadchar)(struct source *);
	char	*(*readline)(struct source *);
	void	(*free)(struct source *);
};

struct source {
	struct vtable	*vtable;
	union {
			FILE *stream;
			struct {
				u_char *buf;
				size_t pos;
			} string;
	} u;
	int		lastchar;
};

void			init_bmachine(void);
void			reset_bmachine(struct source *);
void			scale_number(BIGNUM *, int);
void			normalize(struct number *, u_int);
void			eval(void);
void			pn(const char *, const struct number *);
void			pbn(const char *, const BIGNUM *);
void			negate(struct number *);
void			split_number(const struct number *, BIGNUM *, BIGNUM *);
void			bmul_number(struct number *, struct number *,
			    struct number *);

extern BIGNUM		zero;
