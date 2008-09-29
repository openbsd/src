BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require './test.pl';
}

my $Is_VMS = $^O eq 'VMS';

use Carp qw(carp cluck croak confess);

plan tests => 37;

ok 1;

{ local $SIG{__WARN__} = sub {
    like $_[0], qr/ok (\d+)\n at.+\b(?i:carp\.t) line \d+$/, 'ok 2\n' };

  carp  "ok 2\n";

}

{ local $SIG{__WARN__} = sub {
    like $_[0], qr/(\d+) at.+\b(?i:carp\.t) line \d+$/, 'carp 3' };

  carp 3;

}

sub sub_4 {

local $SIG{__WARN__} = sub {
    like $_[0], qr/^(\d+) at.+\b(?i:carp\.t) line \d+\n\tmain::sub_4\(\) called at.+\b(?i:carp\.t) line \d+$/, 'cluck 4' };

cluck 4;

}

sub_4;

{ local $SIG{__DIE__} = sub {
    like $_[0], qr/^(\d+) at.+\b(?i:carp\.t) line \d+\n\teval \Q{...}\E called at.+\b(?i:carp\.t) line \d+$/, 'croak 5' };

  eval { croak 5 };
}

sub sub_6 {
    local $SIG{__DIE__} = sub {
	like $_[0], qr/^(\d+) at.+\b(?i:carp\.t) line \d+\n\teval \Q{...}\E called at.+\b(?i:carp\.t) line \d+\n\tmain::sub_6\(\) called at.+\b(?i:carp\.t) line \d+$/, 'confess 6' };

    eval { confess 6 };
}

sub_6;

ok(1);

# test for caller_info API
my $eval = "use Carp::Heavy; return Carp::caller_info(0);";
my %info = eval($eval);
is($info{sub_name}, "eval '$eval'", 'caller_info API');

# test for '...::CARP_NOT used only once' warning from Carp::Heavy
my $warning;
eval {
    BEGIN {
	$^W = 1;
	local $SIG{__WARN__} =
	    sub { if( defined $^S ){ warn $_[0] } else { $warning = $_[0] } }
    }
    package Z;
    BEGIN { eval { Carp::croak() } }
};
ok !$warning, q/'...::CARP_NOT used only once' warning from Carp::Heavy/;

# Test the location of error messages.
like(A::short(), qr/^Error at C/, "Short messages skip carped package");

{
    local @C::ISA = "D";
    like(A::short(), qr/^Error at B/, "Short messages skip inheritance");
}

{
    local @D::ISA = "C";
    like(A::short(), qr/^Error at B/, "Short messages skip inheritance");
}

{
    local @D::ISA = "B";
    local @B::ISA = "C";
    like(A::short(), qr/^Error at A/, "Inheritance is transitive");
}

{
    local @B::ISA = "D";
    local @C::ISA = "B";
    like(A::short(), qr/^Error at A/, "Inheritance is transitive");
}

{
    local @C::CARP_NOT = "D";
    like(A::short(), qr/^Error at B/, "Short messages see \@CARP_NOT");
}

{
    local @D::CARP_NOT = "C";
    like(A::short(), qr/^Error at B/, "Short messages see \@CARP_NOT");
}

{
    local @D::CARP_NOT = "B";
    local @B::CARP_NOT = "C";
    like(A::short(), qr/^Error at A/, "\@CARP_NOT is transitive");
}

{
    local @B::CARP_NOT = "D";
    local @C::CARP_NOT = "B";
    like(A::short(), qr/^Error at A/, "\@CARP_NOT is transitive");
}

{
    local @D::ISA = "C";
    local @D::CARP_NOT = "B";
    like(A::short(), qr/^Error at C/, "\@CARP_NOT overrides inheritance");
}

{
    local @D::ISA = "B";
    local @D::CARP_NOT = "C";
    like(A::short(), qr/^Error at B/, "\@CARP_NOT overrides inheritance");
}

# %Carp::Internal
{
    local $Carp::Internal{C} = 1;
    like(A::short(), qr/^Error at B/, "Short doesn't report Internal");
}

{
    local $Carp::Internal{D} = 1;
    like(A::long(), qr/^Error at C/, "Long doesn't report Internal");
}

# %Carp::CarpInternal
{
    local $Carp::CarpInternal{D} = 1;
    like(A::short(), qr/^Error at B/
      , "Short doesn't report calls to CarpInternal");
}

{
    local $Carp::CarpInternal{D} = 1;
    like(A::long(), qr/^Error at C/, "Long doesn't report CarpInternal");
}

# tests for global variables
sub x { carp @_ }
sub w { cluck @_ }

# $Carp::Verbose;
{   my $aref = [
        qr/t at \S*(?i:carp.t) line \d+/,
        qr/t at \S*(?i:carp.t) line \d+\n\s*main::x\('t'\) called at \S*(?i:carp.t) line \d+/
    ];
    my $i = 0;

    for my $re (@$aref) {
        local $Carp::Verbose = $i++;
        local $SIG{__WARN__} = sub {
            like $_[0], $re, 'Verbose';
	};
        package Z;
        main::x('t');
    }
}

# $Carp::MaxEvalLen
{   my $test_num = 1;
    for(0,4) {
        my $txt = "Carp::cluck($test_num)";
        local $Carp::MaxEvalLen = $_;
        local $SIG{__WARN__} = sub {
	    "@_"=~/'(.+?)(?:\n|')/s;
            is length($1), length($_?substr($txt,0,$_):substr($txt,0)), 'MaxEvalLen';
	};
        eval "$txt"; $test_num++;
    }
}

# $Carp::MaxArgLen
{
    for(0,4) {
        my $arg = 'testtest';
        local $Carp::MaxArgLen = $_;
        local $SIG{__WARN__} = sub {
	    "@_"=~/'(.+?)'/;
	    is length($1), length($_?substr($arg,0,$_):substr($arg,0)), 'MaxArgLen';
	};

        package Z;
        main::w($arg);
    }
}

# $Carp::MaxArgNums
{   my $i = 0;
    my $aref = [
        qr/1234 at \S*(?i:carp.t) line \d+\n\s*main::w\(1, 2, 3, 4\) called at \S*(?i:carp.t) line \d+/,
        qr/1234 at \S*(?i:carp.t) line \d+\n\s*main::w\(1, 2, \.\.\.\) called at \S*(?i:carp.t) line \d+/,
    ];

    for(@$aref) {
        local $Carp::MaxArgNums = $i++;
        local $SIG{__WARN__} = sub {
	    like "@_", $_, 'MaxArgNums';
	};

        package Z;
        main::w(1..4);
    }
}

# $Carp::CarpLevel
{   my $i = 0;
    my $aref = [
        qr/1 at \S*(?i:carp.t) line \d+\n\s*main::w\(1\) called at \S*(?i:carp.t) line \d+/,
        qr/1 at \S*(?i:carp.t) line \d+$/,
    ];

    for (@$aref) {
        local $Carp::CarpLevel = $i++;
        local $SIG{__WARN__} = sub {
	    like "@_", $_, 'CarpLevel';
	};

        package Z;
        main::w(1);
    }
}

{
    local $TODO = "VMS exit status semantics don't work this way" if $Is_VMS;

    # Check that croak() and confess() don't clobber $!
    runperl(prog => 'use Carp; $@=q{Phooey}; $!=42; croak(q{Dead})', 
	    stderr => 1);

    is($?>>8, 42, 'croak() doesn\'t clobber $!');

    runperl(prog => 'use Carp; $@=q{Phooey}; $!=42; confess(q{Dead})', 
	    stderr => 1);

    is($?>>8, 42, 'confess() doesn\'t clobber $!');
}

# undef used to be incorrectly reported as the string "undef"
sub cluck_undef {

local $SIG{__WARN__} = sub {
    like $_[0], qr/^Bang! at.+\b(?i:carp\.t) line \d+\n\tmain::cluck_undef\(0, 'undef', 2, undef, 4\) called at.+\b(?i:carp\.t) line \d+$/, "cluck doesn't quote undef" };

cluck "Bang!"

}

cluck_undef (0, "undef", 2, undef, 4);

# line 1 "A"
package A;
sub short {
    B::short();
}

sub long {
    B::long();
}

# line 1 "B"
package B;
sub short {
    C::short();
}

sub long {
    C::long();
}

# line 1 "C"
package C;
sub short {
    D::short();
}

sub long {
    D::long();
}

# line 1 "D"
package D;
sub short {
    eval{ Carp::croak("Error") };
    return $@;
}

sub long {
    eval{ Carp::confess("Error") };
    return $@;
}
