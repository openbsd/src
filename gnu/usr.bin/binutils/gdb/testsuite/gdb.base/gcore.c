/*
 * Test GDB's ability to save and reload a corefile.
 */

#include <stdlib.h>

int extern_array[4] = {1, 2, 3, 4};
static int static_array[4] = {5, 6, 7, 8};
static int un_initialized_array[4];
static char *heap_string;

void 
terminal_func ()
{
  return;
}

void
array_func ()
{
  int local_array[4];
  int i;

  heap_string = (char *) malloc (80);
  strcpy (heap_string, "I'm a little teapot, short and stout...");
  for (i = 0; i < 4; i++)
    {
      un_initialized_array[i] = extern_array[i] + 8;
      local_array[i] = extern_array[i] + 12;
    }
  terminal_func ();
}

#ifdef PROTOTYPES
int factorial_func (int value)
#else
int factorial_func (value)
     int value;
#endif
{
  if (value > 1) {
    value *= factorial_func (value - 1);
  }
  array_func ();
  return (value);
}

main()
{
  factorial_func (6);
  return 0;
}
