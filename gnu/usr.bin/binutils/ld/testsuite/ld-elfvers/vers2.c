/*
 * Test function.  This is built into a shared library, and references a
 * versioned symbol foo that is in test.so.
 */

show_xyzzy()
{
  printf("%d", show_foo());
}
