static int static_foo = 1;
static int static_bar = 2;

/* This padding is just for the benefit of the test harness.  It
   causes the globals to have different addresses than the functions.  */
int dummy[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

int global_foo = 3;
int global_bar = 4;

int
function_foo ()
{
  return 5;
}

int
function_bar ()
{
  return 6;
}
