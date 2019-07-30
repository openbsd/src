use strict;

my $MODULE;

BEGIN {
	$MODULE = (-d "src") ? "Digest::SHA" : "Digest::SHA::PurePerl";
	eval "require $MODULE" || die $@;
	$MODULE->import(qw());
}

BEGIN {
	if ($ENV{PERL_CORE}) {
		chdir 't' if -d 't';
		@INC = '../lib';
	}
}

my $s1 = $MODULE->new;
my $s2 = $MODULE->new;
my $d1 = $s1->add_bits("110")->hexdigest;
my $d2 = $s2->add_bits("1")->add_bits("1")->add_bits("0")->hexdigest;

my $numtests = 1;
print "1..$numtests\n";

for (1 .. $numtests) {
	print "not " unless $d1 eq $d2;
	print "ok ", $_, "\n";
}
