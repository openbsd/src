use strict;

my $MODULE;

BEGIN {
	$MODULE = ($ENV{PERL_CORE} || -d "src") ? "Digest::SHA" : "Digest::SHA::PurePerl";
	eval "require $MODULE" || die $@;
	$MODULE->import(qw(sha1_hex));
}

BEGIN {
	if ($ENV{PERL_CORE}) {
		chdir 't' if -d 't';
		@INC = '../lib';
	}
}

my @vecs = map { eval } <DATA>;
$#vecs -= 2 if $MODULE eq "Digest::SHA::PurePerl";

my $numtests = scalar(@vecs) / 2;
print "1..$numtests\n";

for (1 .. $numtests) {
	my $data = shift @vecs;
	my $digest = shift @vecs;
	print "not " unless sha1_hex($data) eq $digest;
	print "ok ", $_, "\n";
}

__DATA__
"abc"
"a9993e364706816aba3e25717850c26c9cd0d89d"
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
"84983e441c3bd26ebaae4aa1f95129e5e54670f1"
"a" x 1000000
"34aa973cd4c4daa4f61eeb2bdbad27316534016f"
