#!./perl -w

# Tests for the coderef-in-@INC feature

use Config;

my $can_fork   = 0;
my $minitest   = $ENV{PERL_CORE_MINITEST};
my $has_perlio = $Config{useperlio};

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

if (!$minitest) {
    if ($Config{d_fork} && eval 'require POSIX; 1') {
	$can_fork = 1;
    }
}

use strict;
use File::Spec;

require "test.pl";
plan(tests => 49 + !$minitest * (3 + 14 * $can_fork));

sub get_temp_fh {
    my $f = tempfile();
    open my $fh, ">$f" or die "Can't create $f: $!";
    print $fh "package ".substr($_[0],0,-3).";\n1;\n";
    print $fh $_[1] if @_ > 1;
    close $fh or die "Couldn't close: $!";
    open $fh, $f or die "Can't open $f: $!";
    return $fh;
}

sub fooinc {
    my ($self, $filename) = @_;
    if (substr($filename,0,3) eq 'Foo') {
	return get_temp_fh($filename);
    }
    else {
        return undef;
    }
}

push @INC, \&fooinc;

my $evalret = eval { require Bar; 1 };
ok( !$evalret,      'Trying non-magic package' );

$evalret = eval { require Foo; 1 };
die $@ if $@;
ok( $evalret,                      'require Foo; magic via code ref'  );
ok( exists $INC{'Foo.pm'},         '  %INC sees Foo.pm' );
is( ref $INC{'Foo.pm'}, 'CODE',    '  val Foo.pm is a coderef in %INC' );
is( $INC{'Foo.pm'}, \&fooinc,	   '  val Foo.pm is correct in %INC' );

$evalret = eval "use Foo1; 1;";
die $@ if $@;
ok( $evalret,                      'use Foo1' );
ok( exists $INC{'Foo1.pm'},        '  %INC sees Foo1.pm' );
is( ref $INC{'Foo1.pm'}, 'CODE',   '  val Foo1.pm is a coderef in %INC' );
is( $INC{'Foo1.pm'}, \&fooinc,     '  val Foo1.pm is correct in %INC' );

$evalret = eval { do 'Foo2.pl'; 1 };
die $@ if $@;
ok( $evalret,                      'do "Foo2.pl"' );
ok( exists $INC{'Foo2.pl'},        '  %INC sees Foo2.pl' );
is( ref $INC{'Foo2.pl'}, 'CODE',   '  val Foo2.pl is a coderef in %INC' );
is( $INC{'Foo2.pl'}, \&fooinc,     '  val Foo2.pl is correct in %INC' );

pop @INC;


sub fooinc2 {
    my ($self, $filename) = @_;
    if (substr($filename, 0, length($self->[1])) eq $self->[1]) {
	return get_temp_fh($filename);
    }
    else {
        return undef;
    }
}

my $arrayref = [ \&fooinc2, 'Bar' ];
push @INC, $arrayref;

$evalret = eval { require Foo; 1; };
die $@ if $@;
ok( $evalret,                     'Originally loaded packages preserved' );
$evalret = eval { require Foo3; 1; };
ok( !$evalret,                    'Original magic INC purged' );

$evalret = eval { require Bar; 1 };
die $@ if $@;
ok( $evalret,                     'require Bar; magic via array ref' );
ok( exists $INC{'Bar.pm'},        '  %INC sees Bar.pm' );
is( ref $INC{'Bar.pm'}, 'ARRAY',  '  val Bar.pm is an arrayref in %INC' );
is( $INC{'Bar.pm'}, $arrayref,    '  val Bar.pm is correct in %INC' );

ok( eval "use Bar1; 1;",          'use Bar1' );
ok( exists $INC{'Bar1.pm'},       '  %INC sees Bar1.pm' );
is( ref $INC{'Bar1.pm'}, 'ARRAY', '  val Bar1.pm is an arrayref in %INC' );
is( $INC{'Bar1.pm'}, $arrayref,   '  val Bar1.pm is correct in %INC' );

ok( eval { do 'Bar2.pl'; 1 },     'do "Bar2.pl"' );
ok( exists $INC{'Bar2.pl'},       '  %INC sees Bar2.pl' );
is( ref $INC{'Bar2.pl'}, 'ARRAY', '  val Bar2.pl is an arrayref in %INC' );
is( $INC{'Bar2.pl'}, $arrayref,   '  val Bar2.pl is correct in %INC' );

pop @INC;

sub FooLoader::INC {
    my ($self, $filename) = @_;
    if (substr($filename,0,4) eq 'Quux') {
	return get_temp_fh($filename);
    }
    else {
        return undef;
    }
}

my $href = bless( {}, 'FooLoader' );
push @INC, $href;

$evalret = eval { require Quux; 1 };
die $@ if $@;
ok( $evalret,                      'require Quux; magic via hash object' );
ok( exists $INC{'Quux.pm'},        '  %INC sees Quux.pm' );
is( ref $INC{'Quux.pm'}, 'FooLoader',
				   '  val Quux.pm is an object in %INC' );
is( $INC{'Quux.pm'}, $href,        '  val Quux.pm is correct in %INC' );

pop @INC;

my $aref = bless( [], 'FooLoader' );
push @INC, $aref;

$evalret = eval { require Quux1; 1 };
die $@ if $@;
ok( $evalret,                      'require Quux1; magic via array object' );
ok( exists $INC{'Quux1.pm'},       '  %INC sees Quux1.pm' );
is( ref $INC{'Quux1.pm'}, 'FooLoader',
				   '  val Quux1.pm is an object in %INC' );
is( $INC{'Quux1.pm'}, $aref,       '  val Quux1.pm  is correct in %INC' );

pop @INC;

my $sref = bless( \(my $x = 1), 'FooLoader' );
push @INC, $sref;

$evalret = eval { require Quux2; 1 };
die $@ if $@;
ok( $evalret,                      'require Quux2; magic via scalar object' );
ok( exists $INC{'Quux2.pm'},       '  %INC sees Quux2.pm' );
is( ref $INC{'Quux2.pm'}, 'FooLoader',
				   '  val Quux2.pm is an object in %INC' );
is( $INC{'Quux2.pm'}, $sref,       '  val Quux2.pm is correct in %INC' );

pop @INC;

push @INC, sub {
    my ($self, $filename) = @_;
    if (substr($filename,0,4) eq 'Toto') {
	$INC{$filename} = 'xyz';
	return get_temp_fh($filename);
    }
    else {
        return undef;
    }
};

$evalret = eval { require Toto; 1 };
die $@ if $@;
ok( $evalret,                      'require Toto; magic via anonymous code ref'  );
ok( exists $INC{'Toto.pm'},        '  %INC sees Toto.pm' );
ok( ! ref $INC{'Toto.pm'},         q/  val Toto.pm isn't a ref in %INC/ );
is( $INC{'Toto.pm'}, 'xyz',	   '  val Toto.pm is correct in %INC' );

pop @INC;

push @INC, sub {
    my ($self, $filename) = @_;
    if ($filename eq 'abc.pl') {
	return get_temp_fh($filename, qq(return "abc";\n));
    }
    else {
	return undef;
    }
};

my $ret = "";
$ret ||= do 'abc.pl';
is( $ret, 'abc', 'do "abc.pl" sees return value' );

{
    my $filename = $^O eq 'MacOS' ? ':Foo:Foo.pm' : './Foo.pm';
    #local @INC; # local fails on tied @INC
    my @old_INC = @INC; # because local doesn't work on tied arrays
    @INC = sub { $filename = 'seen'; return undef; };
    eval { require $filename; };
    is( $filename, 'seen', 'the coderef sees fully-qualified pathnames' );
    @INC = @old_INC;
}

# this will segfault if it fails

sub PVBM () { 'foo' }
{ my $dummy = index 'foo', PVBM }

# I don't know whether these requires should succeed or fail. 5.8 failed
# all of them; 5.10 with an ordinary constant in place of PVBM lets the
# latter two succeed. For now I don't care, as long as they don't
# segfault :).

unshift @INC, sub { PVBM };
eval 'require foo';
ok( 1, 'returning PVBM doesn\'t segfault require' );
eval 'use foo';
ok( 1, 'returning PVBM doesn\'t segfault use' );
shift @INC;
unshift @INC, sub { \PVBM };
eval 'require foo';
ok( 1, 'returning PVBM ref doesn\'t segfault require' );
eval 'use foo';
ok( 1, 'returning PVBM ref doesn\'t segfault use' );
shift @INC;

exit if $minitest;

SKIP: {
    skip( "No PerlIO available", 3 ) unless $has_perlio;
    pop @INC;

    push @INC, sub {
        my ($cr, $filename) = @_;
        my $module = $filename; $module =~ s,/,::,g; $module =~ s/\.pm$//;
        open my $fh, '<',
             \"package $module; sub complain { warn q() }; \$::file = __FILE__;"
	    or die $!;
        $INC{$filename} = "/custom/path/to/$filename";
        return $fh;
    };

    require Publius::Vergilius::Maro;
    is( $INC{'Publius/Vergilius/Maro.pm'},
        '/custom/path/to/Publius/Vergilius/Maro.pm', '%INC set correctly');
    is( our $file, '/custom/path/to/Publius/Vergilius/Maro.pm',
        '__FILE__ set correctly' );
    {
        my $warning;
        local $SIG{__WARN__} = sub { $warning = shift };
        Publius::Vergilius::Maro::complain();
        like( $warning, qr{something's wrong at /custom/path/to/Publius/Vergilius/Maro.pm}, 'warn() reports correct file source' );
    }
}
pop @INC;

if ($can_fork) {
    require PerlIO::scalar;
    # This little bundle of joy generates n more recursive use statements,
    # with each module chaining the next one down to 0. If it works, then we
    # can safely nest subprocesses
    my $use_filter_too;
    push @INC, sub {
	return unless $_[1] =~ /^BBBLPLAST(\d+)\.pm/;
	my $pid = open my $fh, "-|";
	if ($pid) {
	    # Parent
	    return $fh unless $use_filter_too;
	    # Try filters and state in addition.
	    return ($fh, sub {s/$_[1]/pass/; return}, "die")
	}
	die "Can't fork self: $!" unless defined $pid;

	# Child
	my $count = $1;
	# Lets force some fun with odd sized reads.
	$| = 1;
	print 'push @main::bbblplast, ';
	print "$count;\n";
	if ($count--) {
	    print "use BBBLPLAST$count;\n";
	}
	if ($use_filter_too) {
	    print "die('In $_[1]');";
	} else {
	    print "pass('In $_[1]');";
	}
	print '"Truth"';
	POSIX::_exit(0);
	die "Can't get here: $!";
    };

    @::bbblplast = ();
    require BBBLPLAST5;
    is ("@::bbblplast", "0 1 2 3 4 5", "All ran");

    foreach (keys %INC) {
	delete $INC{$_} if /^BBBLPLAST/;
    }

    @::bbblplast = ();
    $use_filter_too = 1;

    require BBBLPLAST5;

    is ("@::bbblplast", "0 1 2 3 4 5", "All ran with a filter");
}
