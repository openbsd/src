/*	$OpenBSD: a.h,v 1.3 2001/01/28 19:34:29 niklas Exp $	*/

class B {
public:
	B();
	~B();
	int i;
};

class AA {
	B b;
	char *argstr;
public:
	AA(char *arg);
	~AA();
	int i;
};
