#include <sys/ioccom.h>

struct session_op {
	u_int32_t	cipher;		/* ie. CRYPTO_DES_CBC */
	u_int32_t	mac;		/* ie. CRYPTO_MD5_HMAC */

	u_int32_t	keylen;		/* cipher key */
	caddr_t		key;
	int		mackeylen;	/* mac key */
	caddr_t		mackey;

	u_int32_t	ses;		/* returns: session # */
};

struct crypt_op {
	u_int32_t	ses;
	u_int16_t	op;
	u_int16_t	flags;		/* always 0 */

	u_int		len;
	caddr_t		src, dst;	/* become iov[] inside kernel */
	caddr_t		mac;
	caddr_t		iv;
};

#define COP_ENCRYPT	1
#define COP_DECRYPT	2
/* #define COP_SETKEY	3 */
/* #define COP_GETKEY	4 */

#define	CRIOGET		_IOR('c', 100, u_int32_t)

#define	CIOCGSESSION	_IOWR('c', 101, struct session_op)
#define	CIOCFSESSION	_IOW('c', 102, u_int32_t)
#define CIOCCRYPT	_IOWR('c', 103, struct crypt_op)
