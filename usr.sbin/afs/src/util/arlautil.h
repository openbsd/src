
#ifndef __ARLA_UTIL_H
#define __ARLA_UTIL_H 1


char *copy_basename (const char *s);
char *copy_dirname (const char *s);


/* timeval */

void timevalfix(struct timeval *t1);
void timevaladd(struct timeval *t1, const struct timeval *t2);
void timevalsub(struct timeval *t1, const struct timeval *t2);


#endif /* __ARLA_UTIL_H */
