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
