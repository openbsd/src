/*
  An attempt to replicate PR gdb/574 with a shorter program.

  Printing out *theB failed if the program was compiled with GCC 2.95.
*/

class A {
public:
  virtual void foo() {};		// Stick in a virtual function.
  int a;				// Stick in a data member.
};

class B : public A {
  static int b;				// Stick in a static data member.
};

int main()
{
  B *theB = new B;

  return 0;				// breakpoint: constructs-done
}
