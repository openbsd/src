BEGIN {
	unless ($] >= 5.006) {
		print "1..0\n";
		exit(0);
	}
        if ($ENV{PERL_CORE}) {
                chdir 't' if -d 't';
                @INC = '../lib';
        }
}

print "1..2\n";

require MIME::Base64;

eval {
    my $tmp = MIME::Base64::encode(v300);
    print "# enc: $tmp\n";
};
print "# $@" if $@;
print "not " unless $@;
print "ok 1\n";

require MIME::QuotedPrint;

eval {
    my $tmp = MIME::QuotedPrint::encode(v300);
    print "# enc: $tmp\n";
};
print "# $@" if $@;
print "not " unless $@;
print "ok 2\n";

