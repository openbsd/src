/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ab.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <iostream>

class AB
{
	public:
		AB();
		~AB();
};

AB::AB()
{
   std::cout << "B";
}

AB::~AB()
{
   std::cout << "b";
}

AB b;
