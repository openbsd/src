#!./perl -w

BEGIN {
    $| = 1;
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..18\n";

my $t = 1;
tie my $c => 'Tie::Monitor';

sub ok {
    my($ok, $got, $exp, $rexp, $wexp) = @_;
    my($rgot, $wgot) = (tied $c)->init(0);
    print $ok ? "ok $t\n" : "# expected $exp, got $got\nnot ok $t\n";
    ++$t;
    if ($rexp == $rgot && $wexp == $wgot) {
	print "ok $t\n";
    } else {
	print "# read $rgot expecting $rexp\n" if $rgot != $rexp;
	print "# wrote $wgot expecting $wexp\n" if $wgot != $wexp;
	print "not ok $t\n";
    }
    ++$t;
}

sub ok_undef { ok(!defined($_[0]), shift, "undef", @_) }
sub ok_numeric { ok($_[0] == $_[1], @_) }
sub ok_string { ok($_[0] eq $_[1], @_) }

my($r, $s);
# the thing itself
ok_numeric($r = $c + 0, 0, 1, 0);
ok_string($r = "$c", '0', 1, 0);

# concat
ok_string($c . 'x', '0x', 1, 0);
ok_string('x' . $c, 'x0', 1, 0);
$s = $c . $c;
ok_string($s, '00', 2, 0);
$r = 'x';
$s = $c = $r . 'y';
ok_string($s, 'xy', 1, 1);
$s = $c = $c . 'x';
ok_string($s, '0x', 2, 1);
$s = $c = 'x' . $c;
ok_string($s, 'x0', 2, 1);
$s = $c = $c . $c;
ok_string($s, '00', 3, 1);

# adapted from Tie::Counter by Abigail
package Tie::Monitor;

sub TIESCALAR {
    my($class, $value) = @_;
    bless {
	read => 0,
	write => 0,
	values => [ 0 ],
    };
}

sub FETCH {
    my $self = shift;
    ++$self->{read};
    $self->{values}[$#{ $self->{values} }];
}

sub STORE {
    my($self, $value) = @_;
    ++$self->{write};
    push @{ $self->{values} }, $value;
}

sub init {
    my $self = shift;
    my @results = ($self->{read}, $self->{write});
    $self->{read} = $self->{write} = 0;
    $self->{values} = [ 0 ];
    @results;
}
