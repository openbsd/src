#ifndef _DL_STRING_H_
#define _DL_STRING_H_

#include <linux/types.h>	/* for size_t */

#ifndef NULL
#define NULL ((void *) 0)
#endif

extern inline char *_dl_strcpy (char *dest, const char *src);
extern inline char *_dl_strncpy (char *dest, const char *src, size_t count);
extern inline char *_dl_strcat (char *dest, const char *src);
extern inline char *_dl_strncat (char *dest, const char *src, size_t count);
extern inline int _dl_strcmp (const char *cs, const char *ct);
extern inline int _dl_strncmp (const char *cs, const char *ct, size_t count);
extern inline char *_dl_strchr (const char *s, int c);
extern inline char *_dl_strrchr (const char *s, int c);
extern inline size_t _dl_strspn (const char *cs, const char *ct);
extern inline size_t _dl_strcspn (const char *cs, const char *ct);
extern inline char *_dl_strpbrk (const char *cs, const char *ct);
extern inline char *_dl_strstr (const char *cs, const char *ct);
extern inline size_t _dl_strlen (const char *s);
extern inline char *_dl_strtok (char *s, const char *ct);
extern inline void *_dl_memcpy (void *to, const void *from, size_t n);
extern inline void *_dl_memmove (void *dest, const void *src, size_t n);
extern inline int _dl_memcmp (const void *cs, const void *ct, size_t count);
extern inline void *_dl_memchr (const void *cs, int c, size_t count);
extern inline void *_dl_memset (void *s, int c, size_t count);

/* Make sure we are using our inline version.  */
#define strlen(s) _dl_strlen(s)

extern inline char *
_dl_strcpy (char *dest, const char *src)
{
  char *xdest = dest;

  while ((*dest++ = *src++) != '\0')
    ;
  return xdest;
}

extern inline char *
_dl_strncpy (char *dest, const char *src, size_t n)
{
  char *xdest = dest;

  if (n == 0)
    return xdest;

  while ((*dest = *src++) != '\0')
    if (--n == 0)
      break;
  return xdest;
}

extern inline char *
_dl_strcat (char *dest, const char *src)
{
  char *xdest = dest;

  while (*dest)
    dest++;
  while ((*dest++ = *src++) != '\0')
    ;

  return xdest;
}

extern inline char *
_dl_strncat (char *dest, const char *src, size_t count)
{
  char *xdest = dest;

  if (count)
    {
      while (*dest)
	dest++;
      while ((*dest++ = *src++) != '\0')
	if (--count == 0)
	  break;
    }

  return xdest;
}

extern inline int
_dl_strcmp (const char *cs, const char *ct)
{
  char c;

  while ((c = *cs++) == *ct++)
    if (c == 0)
      return 0;
  return c - *--ct;
}

extern inline int
_dl_strncmp (const char *cs, const char *ct, size_t count)
{
  char c;

  if (!count)
    return 0;

  while ((c = *cs++) == *ct++)
    if (c == 0 || --count == 0)
      return 0;
  return c - *--ct;
}

extern inline char *
_dl_strchr (const char *s, int c)
{
  const char ch = c;

  for (; *s != ch; ++s)
    if (*s == '\0')
      return NULL;
  return (char *) s;
}

extern inline char *
_dl_strrchr (const char * s, int c)
{
  char *res = NULL;
  const char ch = c;

  for (;; ++s)
    {
      if (*s == ch)
	res = (char *) s;
      if (*s == '\0')
	break;
    }
  return res;
}

extern inline size_t
_dl_strspn (const char *s, const char *accept)
{
  const char *p;
  const char *a;
  size_t count = 0;

  for (p = s; *p != '\0'; ++p)
    {
      for (a = accept; *a != '\0'; ++a)
	if (*p == *a)
	  break;
      if (*a == '\0')
	return count;
      ++count;
    }

  return count;
}

extern inline size_t
_dl_strcspn (const char *s, const char *reject)
{
  const char *p;
  const char *a;
  size_t count = 0;

  for (p = s; *p != '\0'; ++p)
    {
      for (a = reject; *a != '\0'; ++a)
	if (*p == *a)
	  return count;
      ++count;
    }

  return count;
}

extern inline char *
_dl_strpbrk (const char *cs, const char *ct)
{
  const char *sc1, *sc2;

  for (sc1 = cs; *sc1 != '\0'; ++sc1)
    for (sc2 = ct; *sc2 != '\0'; ++sc2)
      if (*sc1 == *sc2)
	return (char *) sc1;
  return NULL;
}

extern inline char *
_dl_strstr (const char *cs, const char *ct)
{
  const char *p;

  for (;; cs++)
    {
      for (p = ct; *p == *cs; p++, cs++)
	if (*p == 0)
	  return (char *) cs - (p - ct);
      cs -= p - ct;
      if (*cs == 0)
	return NULL;
    }
}

extern inline size_t
_dl_strlen (const char *s)
{
  const char *sc;

  for (sc = s; *sc != '\0'; ++sc)
    ;
  return sc - s;
}

extern char * ___strtok;

extern inline char *
_dl_strtok(char * s,const char * ct)
{
  char *sbegin, *send;

  sbegin = s ? s : ___strtok;
  if (!sbegin)
    return NULL;
  sbegin += _dl_strspn (sbegin, ct);
  if (*sbegin == '\0')
    {
      ___strtok = NULL;
      return NULL;
    }
  send = _dl_strpbrk (sbegin, ct);
  if (send && *send != '\0')
    *send++ = '\0';
  ___strtok = send;
  return sbegin;
}

extern inline void *
_dl_memcpy (void *to, const void *from, size_t n)
{
  char *cto = to;
  const char *cfrom = from;

  while (n--)
    *cto++ = *cfrom++;
  return to;
}

extern inline void *
_dl_memmove (void *dest, const void *src, size_t n)
{
  char *cdest = dest;
  const char *csrc = src;

  if (dest < src)
    {
      while (n--)
	*cdest++ = *csrc++;
    }
  else
    {
      cdest += n;
      csrc += n;
      while (n--)
	*--cdest = *--csrc;
    }
  return dest;
}

extern inline int
_dl_memcmp (const void *cs, const void *ct, size_t count)
{
  const unsigned char *su1, *su2;

  for (su1 = cs, su2 = ct; count > 0; ++su1, ++su2, count--)
    if (*su1 != *su2)
      return *su1 < *su2 ? -1 : 1;
  return 0;
}

extern inline void *
_dl_memchr (const void *cs, int c, size_t count)
{
  const unsigned char ch = c;
  const unsigned char *s = cs;

  for (; count != 0; ++s, count--)
    if (*s == ch)
      return (void *) s;
  return NULL;
}

extern inline void *
_dl_memset (void *s, int c, size_t count)
{
  char *cs = s;

  while (count--)
    *cs++ = c;
  return s;
}

#endif
