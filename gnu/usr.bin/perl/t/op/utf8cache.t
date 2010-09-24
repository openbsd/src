#!./perl
# Test for malfunctions of utf8 cache

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

unless (eval { require Devel::Peek }) {
    print "# Without Devel::Peek, never mind\n";
    print "1..0\n";
    exit;
}
print "1..1\n";

my $pid = open CHILD, '-|';
die "kablam: $!\n" unless defined $pid;
unless ($pid) {
    open STDERR, ">&STDOUT";
    $a = "hello \x{1234}";
    for (1..2) {
        bar(substr($a, $_, 1));
    }
    sub bar {
        $_[0] = "\x{4321}";
        Devel::Peek::Dump($_[0]);
    }
    exit;
}

{ local $/; $_ = <CHILD> }

my $utf8magic = qr{ ^ \s+ MAGIC \s = .* \n
                      \s+ MG_VIRTUAL \s = .* \n
                      \s+ MG_TYPE \s = \s PERL_MAGIC_utf8 .* \n
                      \s+ MG_LEN \s = .* \n }xm;

if (m{ $utf8magic $utf8magic }x) {
    print "not ";
}
print "ok 1\n";
