/*
 * Testcase to verify that reference to foo@BAR and a definition of foo@@BAR
 * are not treated as a multiple def.
 */
const char * bar1 = "asdf";
const char * bar2 = "asdf";

int bar()
{
	return 3;
}

original_foo()
{
	return 1+bar();

}

old_foo()
{
	return 10+bar();

}

new_foo()
{
	return 1000+bar();

}

main()
{
  old_foo1();
}

__asm__(".symver original_foo,foo@");
__asm__(".symver old_foo,foo@VERS_1.1");
__asm__(".symver old_foo1,foo@VERS_1.2");
__asm__(".symver new_foo,foo@@VERS_1.2");
