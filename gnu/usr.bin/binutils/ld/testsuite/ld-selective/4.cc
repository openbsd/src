struct A
{
  virtual void foo();
  virtual void bar();
};

void A::foo() { }			// lose
void A::bar() { }			// keep

struct B : public A
{
  virtual void foo();
};

void B::foo() { }			// lose

void _start() __asm__("_start");	// keep

A a;					// keep
B b;
A *getme() { return &a; }		// keep

void _start()
{
  getme()->bar();
}

extern "C" void __main() { }
