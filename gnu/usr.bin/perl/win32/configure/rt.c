/*
 Print the default runtime. $Config{libc}
 will be set to this specified value.
*/
#include <stdio.h>
#include <stddef.h>

int main(void) {
#if defined(_UCRT)
  printf("-lucrt\n");
#else
  printf("-lmsvcrt\n");
#endif
  return 0;
}
