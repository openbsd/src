class A {
public:
  class B;
  class C;
};

class A::B {
  int a_b;

public:
  C* get_c(int i);
};

class A::C
{
  int a_c;
};

class E {
public:
  class F;
};
 
class E::F {
public:
  int e_f;
 
  F& operator=(const F &other);
};

void refer_to (E::F *f) {
  // Do nothing.
}

void refer_to (A::C **ref) {
  // Do nothing.  But, while we're at it, force out debug info for
  // A::B and E::F.

  A::B b;
  E::F f;

  refer_to (&f);
}

int main () {
  A::C* c_var;
  A::B* b_var;
  E *e_var;

  // Keep around a reference so that GCC 3.4 doesn't optimize the variable
  // away.
  refer_to (&c_var);
}
