# Adapted from script by Mark Lawrence (ref. rt.cpan.org #94830)

use strict;

my $MODULE;

BEGIN {
	$MODULE = (-d "src") ? "Digest::SHA" : "Digest::SHA::PurePerl";
	eval "require $MODULE" || die $@;
	$MODULE->import(qw(sha1));
}

BEGIN {
	if ($ENV{PERL_CORE}) {
		chdir 't' if -d 't';
		@INC = '../lib';
	}
}

package P1;
use vars qw(@ISA);
@ISA = ($MODULE);

package main;

print "1..1\n";

my $data = 'a';
my $d = P1->new;
print "not " unless $d->add($data)->digest eq sha1($data);
print "ok 1\n";
