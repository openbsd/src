BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

use Carp qw(carp cluck croak confess);

print "1..9\n";

print "ok 1\n";

$SIG{__WARN__} = sub {
    print "ok $1\n"
	if $_[0] =~ m!ok (\d+)\n at .+\b(?i:carp\.t) line \d+$! };

carp  "ok 2\n";
	
$SIG{__WARN__} = sub {
    print "ok $1\n"
	if $_[0] =~ m!(\d+) at .+\b(?i:carp\.t) line \d+$! };

carp 3;

sub sub_4 {

$SIG{__WARN__} = sub {
    print "ok $1\n"
	if $_[0] =~ m!^(\d+) at .+\b(?i:carp\.t) line \d+\n\tmain::sub_4\(\) called at .+\b(?i:carp\.t) line \d+$! };

cluck 4;

}

sub_4;

$SIG{__DIE__} = sub {
    print "ok $1\n"
	if $_[0] =~ m!^(\d+) at .+\b(?i:carp\.t) line \d+\n\teval \Q{...}\E called at .+\b(?i:carp\.t) line \d+$! };

eval { croak 5 };

sub sub_6 {
    $SIG{__DIE__} = sub {
	print "ok $1\n"
	    if $_[0] =~ m!^(\d+) at .+\b(?i:carp\.t) line \d+\n\teval \Q{...}\E called at .+\b(?i:carp\.t) line \d+\n\tmain::sub_6\(\) called at .+\b(?i:carp\.t) line \d+$! };

    eval { confess 6 };
}

sub_6;

print "ok 7\n";

# test for caller_info API
my $eval = "use Carp::Heavy; return Carp::caller_info(0);";
my %info = eval($eval);
print "not " if ($info{sub_name} ne "eval '$eval'");
print "ok 8\n";

# test for '...::CARP_NOT used only once' warning from Carp::Heavy
my $warning;
eval {
    BEGIN {
	$^W = 1;
	$SIG{__WARN__} =
	    sub { if( defined $^S ){ warn $_[0] } else { $warning = $_[0] } }
    }
    package Z; 
    BEGIN { eval { Carp::croak() } }
};
print $warning ? "not ok 9\n#$warning" : "ok 9\n";
