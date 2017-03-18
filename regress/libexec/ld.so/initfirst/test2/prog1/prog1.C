/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: prog1.C,v 1.2 2017/03/18 16:58:22 kettenis Exp $
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

int
main()
{
	return 0;
}
