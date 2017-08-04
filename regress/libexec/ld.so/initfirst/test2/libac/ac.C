/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ac.C,v 1.2 2017/08/04 18:26:54 kettenis Exp $
 */

#include <cstdio>

class AC
{
	public:
		AC();
		~AC();
};

AC::AC()
{
   std::printf("C");
}

AC::~AC()
{
   std::printf("c");
}

AC c;
