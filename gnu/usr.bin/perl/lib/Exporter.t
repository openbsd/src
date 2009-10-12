#!perl -w

BEGIN {
   if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

# Can't use Test::Simple/More, they depend on Exporter.
my $test;
sub ok ($;$) {
    my($ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    printf "%sok %d%s\n", ($ok ? '' : 'not '), $test,
      (defined $name ? " - $name" : '');

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;
    
    $test++;
    return $ok;
}


BEGIN {
    $test = 1;
    print "1..30\n";
    require Exporter;
    ok( 1, 'Exporter compiled' );
}


BEGIN {
    # Methods which Exporter says it implements.
    @Exporter_Methods = qw(import
                           export_to_level
                           require_version
                           export_fail
                          );
}


package Testing;
require Exporter;
@ISA = qw(Exporter);

# Make sure Testing can do everything its supposed to.
foreach my $meth (@::Exporter_Methods) {
    ::ok( Testing->can($meth), "subclass can $meth()" );
}

%EXPORT_TAGS = (
                This => [qw(stuff %left)],
                That => [qw(Above the @wailing)],
                tray => [qw(Fasten $seatbelt)],
               );
@EXPORT    = qw(lifejacket is);
@EXPORT_OK = qw(under &your $seat);
$VERSION = '1.05';

::ok( Testing->require_version(1.05),   'require_version()' );
eval { Testing->require_version(1.11); 1 };
::ok( $@,                               'require_version() fail' );
::ok( Testing->require_version(0),      'require_version(0)' );

sub lifejacket  { 'lifejacket'  }
sub stuff       { 'stuff'       }
sub Above       { 'Above'       }
sub the         { 'the'         }
sub Fasten      { 'Fasten'      }
sub your        { 'your'        }
sub under       { 'under'       }
use vars qw($seatbelt $seat @wailing %left);
$seatbelt = 'seatbelt';
$seat     = 'seat';
@wailing = qw(AHHHHHH);
%left = ( left => "right" );

BEGIN {*is = \&Is};
sub Is { 'Is' };

Exporter::export_ok_tags();

my %tags     = map { $_ => 1 } map { @$_ } values %EXPORT_TAGS;
my %exportok = map { $_ => 1 } @EXPORT_OK;
my $ok = 1;
foreach my $tag (keys %tags) {
    $ok = exists $exportok{$tag};
}
::ok( $ok, 'export_ok_tags()' );


package Foo;
Testing->import;

::ok( defined &lifejacket,      'simple import' );

my $got = eval {&lifejacket};
::ok ( $@ eq "", 'check we can call the imported subroutine')
  or print STDERR "# \$\@ is $@\n";
::ok ( $got eq 'lifejacket', 'and that it gave the correct result')
  or print STDERR "# expected 'lifejacket', got " .
  (defined $got ? "'$got'" : "undef") . "\n";

# The string eval is important. It stops $Foo::{is} existing when
# Testing->import is called.
::ok( eval "defined &is",
      "Import a subroutine where exporter must create the typeglob" );
$got = eval "&is";
::ok ( $@ eq "", 'check we can call the imported autoloaded subroutine')
  or chomp ($@), print STDERR "# \$\@ is $@\n";
::ok ( $got eq 'Is', 'and that it gave the correct result')
  or print STDERR "# expected 'Is', got " .
  (defined $got ? "'$got'" : "undef") . "\n";


package Bar;
my @imports = qw($seatbelt &Above stuff @wailing %left);
Testing->import(@imports);

::ok( (!grep { eval "!defined $_" } map({ /^\w/ ? "&$_" : $_ } @imports)),
      'import by symbols' );


package Yar;
my @tags = qw(:This :tray);
Testing->import(@tags);

::ok( (!grep { eval "!defined $_" } map { /^\w/ ? "&$_" : $_ }
             map { @$_ } @{$Testing::EXPORT_TAGS{@tags}}),
      'import by tags' );


package Arrr;
Testing->import(qw(!lifejacket));

::ok( !defined &lifejacket,     'deny import by !' );


package Mars;
Testing->import('/e/');

::ok( (!grep { eval "!defined $_" } map { /^\w/ ? "&$_" : $_ }
            grep { /e/ } @Testing::EXPORT, @Testing::EXPORT_OK),
      'import by regex');


package Venus;
Testing->import('!/e/');

::ok( (!grep { eval "defined $_" } map { /^\w/ ? "&$_" : $_ }
            grep { /e/ } @Testing::EXPORT, @Testing::EXPORT_OK),
      'deny import by regex');
::ok( !defined &lifejacket, 'further denial' );


package More::Testing;
@ISA = qw(Exporter);
$VERSION = 0;
eval { More::Testing->require_version(0); 1 };
::ok(!$@,       'require_version(0) and $VERSION = 0');


package Yet::More::Testing;
@ISA = qw(Exporter);
$VERSION = 0;
eval { Yet::More::Testing->require_version(10); 1 };
::ok($@ !~ /\(undef\)/,       'require_version(10) and $VERSION = 0');


my $warnings;
BEGIN {
    local $SIG{__WARN__} = sub { $warnings = join '', @_ };
    package Testing::Unused::Vars;
    @ISA = qw(Exporter);
    @EXPORT = qw(this $TODO that);

    package Foo;
    Testing::Unused::Vars->import;
}

::ok( !$warnings, 'Unused variables can be exported without warning' ) ||
  print "# $warnings\n";

package Moving::Target;
@ISA = qw(Exporter);
@EXPORT_OK = qw (foo);

sub foo {"This is foo"};
sub bar {"This is bar"};

package Moving::Target::Test;

Moving::Target->import ('foo');

::ok (foo() eq "This is foo", "imported foo before EXPORT_OK changed");

push @Moving::Target::EXPORT_OK, 'bar';

Moving::Target->import ('bar');

::ok (bar() eq "This is bar", "imported bar after EXPORT_OK changed");

package The::Import;

use Exporter 'import';

::ok(\&import == \&Exporter::import, "imported the import routine");

@EXPORT = qw( wibble );
sub wibble {return "wobble"};

package Use::The::Import;

The::Import->import;

my $val = eval { wibble() };
::ok($val eq "wobble", "exported importer worked");

# Check that Carp recognizes Exporter as internal to Perl 
require Carp;
::ok($Carp::Internal{Exporter}, "Carp recognizes Exporter");
::ok($Carp::Internal{'Exporter::Heavy'}, "Carp recognizes Exporter::Heavy");

