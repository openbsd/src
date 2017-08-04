/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: aa.C,v 1.2 2017/08/04 18:26:54 kettenis Exp $
 */

#include <cstdio>

class AA
{
	public:
		AA();
		~AA();
};

AA::AA()
{
   std::printf("A");
}

AA::~AA()
{
   std::printf("a");
}

AA a;
