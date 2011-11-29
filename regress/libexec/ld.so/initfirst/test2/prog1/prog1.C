/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: prog1.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <iostream>

class P
{
	public:
		P();
		~P();
};

P::P()
{
	std::cout << "P";
}

P::~P()
{
	std::cout << "p";
}

P p;

main()
{
	return 0;
}
