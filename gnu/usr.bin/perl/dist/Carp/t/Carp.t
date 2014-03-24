use warnings;
no warnings "once";
use Config;

use IPC::Open3 1.0103 qw(open3);
use Test::More tests => 61;

sub runperl {
    my(%args) = @_;
    my($w, $r);
    my $pid = open3($w, $r, undef, $^X, "-e", $args{prog});
    close $w;
    my $output = "";
    while(<$r>) { $output .= $_; }
    waitpid($pid, 0);
    return $output;
}

my $Is_VMS = $^O eq 'VMS';

use Carp qw(carp cluck croak confess);

BEGIN {
    # This test must be run at BEGIN time, because code later in this file
    # sets CORE::GLOBAL::caller
    ok !exists $CORE::GLOBAL::{caller},
        "Loading doesn't create CORE::GLOBAL::caller";
}

{
  my $str = Carp::longmess("foo");
  is(
    $str,
    "foo at t/Carp.t line 31.\n",
    "we don't overshoot the top stack frame",
  );
}

{
    local $SIG{__WARN__} = sub {
        like $_[0], qr/ok (\d+)\n at.+\b(?i:carp\.t) line \d+\.$/, 'ok 2\n';
    };

    carp "ok 2\n";
}

{
    local $SIG{__WARN__} = sub {
        like $_[0], qr/(\d+) at.+\b(?i:carp\.t) line \d+\.$/, 'carp 3';
    };

    carp 3;
}

sub sub_4 {
    local $SIG{__WARN__} = sub {
        like $_[0],
            qr/^(\d+) at.+\b(?i:carp\.t) line \d+\.\n\tmain::sub_4\(\) called at.+\b(?i:carp\.t) line \d+$/,
            'cluck 4';
    };

    cluck 4;
}

sub_4;

{
    local $SIG{__DIE__} = sub {
        like $_[0],
            qr/^(\d+) at.+\b(?i:carp\.t) line \d+\.\n\teval \Q{...}\E called at.+\b(?i:carp\.t) line \d+$/,
            'croak 5';
    };

    eval { croak 5 };
}

sub sub_6 {
    local $SIG{__DIE__} = sub {
        like $_[0],
            qr/^(\d+) at.+\b(?i:carp\.t) line \d+\.\n\teval \Q{...}\E called at.+\b(?i:carp\.t) line \d+\n\tmain::sub_6\(\) called at.+\b(?i:carp\.t) line \d+$/,
            'confess 6';
    };

    eval { confess 6 };
}

sub_6;

ok(1);

# test for caller_info API
my $eval = "use Carp; return Carp::caller_info(0);";
my %info = eval($eval);
is( $info{sub_name}, "eval '$eval'", 'caller_info API' );

# test for '...::CARP_NOT used only once' warning from Carp
my $warning;
eval {
    BEGIN {
        local $SIG{__WARN__} = sub {
            if   ( defined $^S ) { warn $_[0] }
            else                 { $warning = $_[0] }
            }
    }

    package Z;

    BEGIN {
        eval { Carp::croak() };
    }
};
ok !$warning, q/'...::CARP_NOT used only once' warning from Carp/;

# Test the location of error messages.
like( A::short(), qr/^Error at C/, "Short messages skip carped package" );

{
    local @C::ISA = "D";
    like( A::short(), qr/^Error at B/, "Short messages skip inheritance" );
}

{
    local @D::ISA = "C";
    like( A::short(), qr/^Error at B/, "Short messages skip inheritance" );
}

{
    local @D::ISA = "B";
    local @B::ISA = "C";
    like( A::short(), qr/^Error at A/, "Inheritance is transitive" );
}

{
    local @B::ISA = "D";
    local @C::ISA = "B";
    like( A::short(), qr/^Error at A/, "Inheritance is transitive" );
}

{
    local @C::CARP_NOT = "D";
    like( A::short(), qr/^Error at B/, "Short messages see \@CARP_NOT" );
}

{
    local @D::CARP_NOT = "C";
    like( A::short(), qr/^Error at B/, "Short messages see \@CARP_NOT" );
}

{
    local @D::CARP_NOT = "B";
    local @B::CARP_NOT = "C";
    like( A::short(), qr/^Error at A/, "\@CARP_NOT is transitive" );
}

{
    local @B::CARP_NOT = "D";
    local @C::CARP_NOT = "B";
    like( A::short(), qr/^Error at A/, "\@CARP_NOT is transitive" );
}

{
    local @D::ISA      = "C";
    local @D::CARP_NOT = "B";
    like( A::short(), qr/^Error at C/, "\@CARP_NOT overrides inheritance" );
}

{
    local @D::ISA      = "B";
    local @D::CARP_NOT = "C";
    like( A::short(), qr/^Error at B/, "\@CARP_NOT overrides inheritance" );
}

# %Carp::Internal
{
    local $Carp::Internal{C} = 1;
    like( A::short(), qr/^Error at B/, "Short doesn't report Internal" );
}

{
    local $Carp::Internal{D} = 1;
    like( A::long(), qr/^Error at C/, "Long doesn't report Internal" );
}

# %Carp::CarpInternal
{
    local $Carp::CarpInternal{D} = 1;
    like(
        A::short(), qr/^Error at B/,
        "Short doesn't report calls to CarpInternal"
    );
}

{
    local $Carp::CarpInternal{D} = 1;
    like( A::long(), qr/^Error at C/, "Long doesn't report CarpInternal" );
}

# tests for global variables
sub x { carp @_ }
sub w { cluck @_ }

# $Carp::Verbose;
{
    my $aref = [
        qr/t at \S*(?i:carp.t) line \d+\./,
        qr/t at \S*(?i:carp.t) line \d+\.\n\s*main::x\('t'\) called at \S*(?i:carp.t) line \d+/
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
{
    my $test_num = 1;
    for ( 0, 4 ) {
        my $txt = "Carp::cluck($test_num)";
        local $Carp::MaxEvalLen = $_;
        local $SIG{__WARN__} = sub {
            "@_" =~ /'(.+?)(?:\n|')/s;
            is length($1),
                length( $_ ? substr( $txt, 0, $_ ) : substr( $txt, 0 ) ),
                'MaxEvalLen';
        };
        eval "$txt";
        $test_num++;
    }
}

# $Carp::MaxArgLen
{
    for ( 0, 4 ) {
        my $arg = 'testtest';
        local $Carp::MaxArgLen = $_;
        local $SIG{__WARN__} = sub {
            "@_" =~ /'(.+?)'/;
            is length($1),
                length( $_ ? substr( $arg, 0, $_ ) : substr( $arg, 0 ) ),
                'MaxArgLen';
        };

        package Z;
        main::w($arg);
    }
}

# $Carp::MaxArgNums
{
    my $i    = 0;
    my $aref = [
        qr/1234 at \S*(?i:carp.t) line \d+\.\n\s*main::w\(1, 2, 3, 4\) called at \S*(?i:carp.t) line \d+/,
        qr/1234 at \S*(?i:carp.t) line \d+\.\n\s*main::w\(1, 2, \.\.\.\) called at \S*(?i:carp.t) line \d+/,
    ];

    for (@$aref) {
        local $Carp::MaxArgNums = $i++;
        local $SIG{__WARN__} = sub {
            like "@_", $_, 'MaxArgNums';
        };

        package Z;
        main::w( 1 .. 4 );
    }
}

# $Carp::CarpLevel
{
    my $i    = 0;
    my $aref = [
        qr/1 at \S*(?i:carp.t) line \d+\.\n\s*main::w\(1\) called at \S*(?i:carp.t) line \d+/,
        qr/1 at \S*(?i:carp.t) line \d+\.$/,
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

SKIP:
{
    skip "IPC::Open3::open3 needs porting", 2 if $Is_VMS;

    # Check that croak() and confess() don't clobber $!
    runperl(
        prog   => 'use Carp; $@=q{Phooey}; $!=42; croak(q{Dead})',
        stderr => 1
    );

    is( $? >> 8, 42, 'croak() doesn\'t clobber $!' );

    runperl(
        prog   => 'use Carp; $@=q{Phooey}; $!=42; confess(q{Dead})',
        stderr => 1
    );

    is( $? >> 8, 42, 'confess() doesn\'t clobber $!' );
}

# undef used to be incorrectly reported as the string "undef"
sub cluck_undef {

    local $SIG{__WARN__} = sub {
        like $_[0],
            qr/^Bang! at.+\b(?i:carp\.t) line \d+\.\n\tmain::cluck_undef\(0, 'undef', 2, undef, 4\) called at.+\b(?i:carp\.t) line \d+$/,
            "cluck doesn't quote undef";
    };

    cluck "Bang!"

}

cluck_undef( 0, "undef", 2, undef, 4 );

# check that Carp respects CORE::GLOBAL::caller override after Carp
# has been compiled
for my $bodge_job ( 2, 1, 0 ) { SKIP: {
    skip "can't safely detect incomplete caller override on perl $]", 6
	if $bodge_job && !Carp::CALLER_OVERRIDE_CHECK_OK;
    print '# ', ( $bodge_job ? 'Not ' : '' ),
        "setting \@DB::args in caller override\n";
    if ( $bodge_job == 1 ) {
        require B;
        print "# required B\n";
    }
    my $accum = '';
    local *CORE::GLOBAL::caller = sub {
        local *__ANON__ = "fakecaller";
        my @c = CORE::caller(@_);
        $c[0] ||= 'undef';
        $accum .= "@c[0..3]\n";
        if ( !$bodge_job && CORE::caller() eq 'DB' ) {

            package DB;
            return CORE::caller( ( $_[0] || 0 ) + 1 );
        }
        else {
            return CORE::caller( ( $_[0] || 0 ) + 1 );
        }
    };
    eval "scalar caller()";
    like( $accum, qr/main::fakecaller/,
        "test CORE::GLOBAL::caller override in eval" );
    $accum = '';
    my $got = A::long(42);
    like( $accum, qr/main::fakecaller/,
        "test CORE::GLOBAL::caller override in Carp" );
    my $package = 'A';
    my $where = $bodge_job == 1 ? ' in &main::__ANON__' : '';
    my $warning
        = $bodge_job
        ? "\Q** Incomplete caller override detected$where; \@DB::args were not set **\E"
        : '';

    for ( 0 .. 2 ) {
        my $previous_package = $package;
        ++$package;
        like( $got,
            qr/${package}::long\($warning\) called at $previous_package line \d+/,
            "Correct arguments for $package" );
    }
    my $arg = $bodge_job ? $warning : 42;
    like(
        $got, qr!A::long\($arg\) called at.+\b(?i:carp\.t) line \d+!,
        'Correct arguments for A'
    );
} }

SKIP: {
    skip "can't safely detect incomplete caller override on perl $]", 1
	unless Carp::CALLER_OVERRIDE_CHECK_OK;
    eval q{
	no warnings 'redefine';
	sub CORE::GLOBAL::caller {
	    my $height = $_[0];
	    $height++;
	    return CORE::caller($height);
	}
    };

    my $got = A::long(42);

    like(
	$got,
	qr!A::long\(\Q** Incomplete caller override detected; \E\@DB::args\Q were not set **\E\) called at.+\b(?i:carp\.t) line \d+!,
	'Correct arguments for A'
    );
}

# UTF8-flagged strings should not cause Carp to try to load modules (even
# implicitly via utf8_heavy.pl) after a syntax error [perl #82854].
SKIP:
{
    skip "IPC::Open3::open3 needs porting", 1 if $Is_VMS;
    like(
      runperl(
        prog => q<
          use utf8; use strict; use Carp;
          BEGIN { $SIG{__DIE__} = sub { Carp::croak qq(aaaaa$_[0]) } }
          $c
        >,
        stderr=>1,
      ),
      qr/aaaaa/,
      'Carp can handle UTF8-flagged strings after a syntax error',
    );
}

SKIP:
{
    skip "IPC::Open3::open3 needs porting", 1 if $Is_VMS;
    skip("B:: always created when static", 1)
      if $Config{static_ext} =~ /\bB\b/;
    is(
      runperl(
	prog => q<
	  use Carp;
	  $SIG{__WARN__} = sub{};
	  carp (qq(A duck, but which duck?));
	  print q(ok) unless exists $::{q(B::)};
	>,
      ),
      'ok',
      'Carp does not autovivify *B::',
    );
}

# [perl #96672]
<D::DATA> for 1..2;
eval { croak 'heek' };
$@ =~ s/\n.*//; # just check first line
is $@, "heek at ".__FILE__." line ".(__LINE__-2).", <DATA> line 2.\n",
    'last handle line num is mentioned';

SKIP:
{
    skip "IPC::Open3::open3 needs porting", 1 if $Is_VMS;
    like(
      runperl(
        prog => q<
          open FH, q-Makefile.PL-;
          <FH>;  # set PL_last_in_gv
          BEGIN { *CORE::GLOBAL::die = sub { die Carp::longmess(@_) } };
          use Carp;
          die fumpts;
        >,
      ),
      qr 'fumpts',
      'Carp::longmess works inside CORE::GLOBAL::die',
    );
}

# New tests go here

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
    eval { Carp::croak("Error") };
    return $@;
}

sub long {
    eval { Carp::confess("Error") };
    return $@;
}

# Put new tests at "new tests go here"
__DATA__
1
2
3
