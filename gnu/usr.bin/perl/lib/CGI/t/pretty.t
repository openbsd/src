#!/usr/local/bin/perl -w

BEGIN {
	chdir 't' if -d 't';
	if ($ENV{PERL_CORE}) {
		@INC = '../lib';
	} else {
		unshift @INC, qw( ../blib/lib ../blib/arch lib );
	}
}

use strict;
use Test::More tests => 5;

BEGIN { use_ok('CGI::Pretty') };

# This is silly use_ok should take arguments
use CGI::Pretty (':all');

is(h1(), '<h1>',"single tag");

is(ol(li('fred'),li('ethel')), <<HTML,   "basic indentation");
<ol>
	<li>
		fred
	</li>
	<li>
		ethel
	</li>
</ol>
HTML


is(p('hi',pre('there'),'frog'), <<HTML, "<pre> tags");
<p>
	hi <pre>there</pre>
	frog
</p>
HTML


is(p('hi',a({-href=>'frog'},'there'),'frog'), <<HTML,   "as-is");
<p>
	hi <a href="frog">there</a>
	frog
</p>
HTML

