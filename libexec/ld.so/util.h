#ifndef __DL_UTIL_H__
#define __DL_UTIL_H__
int _dl_write __P((int, const char *, int));
void *_dl_malloc(const int size);
void _dl_free(void *);
char *_dl_strdup(const char *);
void _dl_printf(const char *fmt, ...);

/*
 *	The following functions are declared inline so they can
 *	be used before bootstrap linking has been finished.
 */
static inline void
_dl_wrstderr(const char *s)
{
	while(*s) {
		_dl_write(2, s, 1);
		s++;
	}
}

static inline void *
_dl_memset(void *p, const char v, size_t c)
{
	char *ip = p;

	while(c--)
		*ip++ = v;
	return(p);
}

static inline int
_dl_strlen(const char *p)
{
	const char *s = p;

	while(*s != '\0')
		s++;
	return(s - p);
}

static inline char *
_dl_strcpy(char *d, const char *s)
{
	char *rd = d;

	while((*d++ = *s++) != '\0');

	return(rd);
}

static inline int
_dl_strncmp(const char *d, const char *s, int c)
{
	while(c-- && *d && *d == *s) {
		d++;
		s++;
	};
	if(c < 0) {
		return(0);
	}
	return(*d - *s);
}
 
static inline int
_dl_strcmp(const char *d, const char *s)
{
	while(*d && *d == *s) {
		d++;
		s++;
	}
	return(*d - *s);
}
 
static inline const char *
_dl_strchr(const char *p, const int c)
{
	while(*p) {
		if(*p == c) {
			return(p);
		}
		p++;
	}
	return(0);
}

#endif /*__DL_UTIL_H__*/
