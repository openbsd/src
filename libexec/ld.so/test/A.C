#include "iostream.h"
#include "a.h"

AA::AA()
{
	cout << "A constructor\n";
}
AA::~AA()
{
	cout << "A destructor\n";
}

AA a;
AA d;
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
