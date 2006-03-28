#!./perl

print "1..15\n";

$SIG{__DIE__} = sub { print ref($_[0]) ? ("ok ",$_[0]->[0]++,"\n") : @_ } ;

$err = "#[\000]\nok 1\n";
eval {
    die $err;
};

print "not " unless $@ eq $err;
print "ok 2\n";

$x = [3];
eval { die $x; };

print "not " unless $x->[0] == 4;
print "ok 4\n";

eval {
    eval {
	die [ 5 ];
    };
    die if $@;
};

eval {
    eval {
	die bless [ 7 ], "Error";
    };
    die if $@;
};

print "not " unless ref($@) eq "Out";
print "ok 10\n";

{
    package Error;

    sub PROPAGATE {
	print "ok ",$_[0]->[0]++,"\n";
	bless [$_[0]->[0]], "Out";
    }
}

{
    # die/warn and utf8
    use utf8;
    local $SIG{__DIE__};
    my $msg = "ce ºtii tu, bã ?\n";
    eval { die $msg }; print "not " unless $@ eq $msg;
    print "ok 11\n";
    our $err;
    local $SIG{__WARN__} = $SIG{__DIE__} = sub { $err = shift };
    eval { die $msg }; print "not " unless $err eq $msg;
    print "ok 12\n";
    eval { warn $msg }; print "not " unless $err eq $msg;
    print "ok 13\n";
    eval qq/ use strict; \$\x{3b1} /;
    print "not " unless $@ =~ /Global symbol "\$\x{3b1}"/;
    print "ok 14\n";
}

# [perl #36470] got uninit warning if $@ was undef

{
    my $ok = 1;
    local $SIG{__DIE__};
    local $SIG{__WARN__} = sub { $ok = 0 };
    eval { undef $@; die };
    print "not " unless $ok;
    print "ok 15\n";
}
