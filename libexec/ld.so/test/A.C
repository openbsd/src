#include "iostream.h"
#include "a.h"

AA::AA(char *arg)
{
	cout << "A constructor " << arg << "\n";
	argstr = arg;
}
AA::~AA()
{
	cout << "A destructor " << argstr << "\n";
}

AA a("a");;
AA b("b");;
/*
AA f;
*/

B::B()
{
	cout << "B constructor\n";
}
B::~B()
{
	cout << "B destructor\n";
}
