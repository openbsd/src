/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ab.C,v 1.2 2017/08/04 18:26:54 kettenis Exp $
 */

#include <cstdio>

class AB
{
	public:
		AB();
		~AB();
};

AB::AB()
{
   std::printf("B");
}

AB::~AB()
{
   std::printf("b");
}

AB b;
