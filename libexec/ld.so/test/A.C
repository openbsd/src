/*	$OpenBSD: A.C,v 1.3 2001/01/28 19:34:29 niklas Exp $	*/

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
