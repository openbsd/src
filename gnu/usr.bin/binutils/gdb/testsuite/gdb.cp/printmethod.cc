/* Create some objects, and try to print out their methods.  */

class A {
public:
  virtual void virt() {};
  void nonvirt() {};
};

int main()
{
  A *theA = new A;

  return 0;				// breakpoint: constructs-done
}
