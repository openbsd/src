#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest;
use File::Spec;
use IO::File;
use Config;

# Don't let our own verbosity/test_file get mixed up with our subprocess's
my @makefile_keys = qw(TEST_VERBOSE HARNESS_VERBOSE TEST_FILES MAKEFLAGS);
local  @ENV{@makefile_keys};
delete @ENV{@makefile_keys};

my @makefile_types = qw(small passthrough traditional);
my $tests_per_type = 14;
if ( $Config{make} && find_in_path($Config{make}) ) {
    plan tests => 38 + @makefile_types*$tests_per_type*2;
} else {
    plan skip_all => "Don't know how to invoke 'make'";
}
ok 1, "Loaded";


#########################

use Cwd ();
my $cwd = Cwd::cwd;
my $tmp = MBTest->tmpdir;

# Create test distribution; set requires and build_requires
use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

chdir( $dist->dirname ) or die "Can't chdir to '@{[$dist->dirname]}': $!";


#########################

use Module::Build;
use Module::Build::Compat;

use Carp;  $SIG{__WARN__} = \&Carp::cluck;

my @make = $Config{make} eq 'nmake' ? ('nmake', '-nologo') : ($Config{make});

#########################

# Test without requires

test_makefile_types();

# Test with requires

my $distname = $dist->name;
$dist->change_build_pl({ 
  module_name         => $distname,
  license             => 'perl',
  requires            => {
    'perl'        => $],
    'File::Spec'  => 0,
  },
  build_requires      => {
    'Test::More'  => 0,
  },
});

$dist->regen;

test_makefile_types( requires => {
    'perl' => $],
    'File::Spec' => 0,
    'Test::More' => 0,
});

######################

$dist->change_build_pl({ 
  module_name         => $distname,
  license             => 'perl',
});
$dist->regen;

# Create M::B instance but don't pollute STDOUT
my $mb;
stdout_of( sub {
    $mb = Module::Build->new_from_context;
});
ok $mb, "Module::Build->new_from_context";


{
  # Make sure fake_makefile() can run without 'build_class', as it may be
  # in older-generated Makefile.PLs
  my $warning = '';
  local $SIG{__WARN__} = sub { $warning = shift; };
  my $maketext = eval { Module::Build::Compat->fake_makefile(makefile => 'Makefile') };
  is $@, '', "fake_makefile lived";
  like $maketext, qr/^realclean/m, "found 'realclean' in fake_makefile output";
  like $warning, qr/build_class/, "saw warning about 'build_class'";
}

{
  # Make sure custom builder subclass is used in the created
  # Makefile.PL - make sure it fails in the right way here.
  local @Foo::Builder::ISA = qw(Module::Build);
  my $foo_builder;
  stdout_of( sub {
    $foo_builder = Foo::Builder->new_from_context;
  });
  foreach my $style ('passthrough', 'small') {
    Module::Build::Compat->create_makefile_pl($style, $foo_builder);
    ok -e 'Makefile.PL', "$style Makefile.PL created";
    
    # Should fail with "can't find Foo/Builder.pm"
    my $result;
    my ($stdout, $stderr ) = stdout_stderr_of (sub {
      $result = $mb->run_perl_script('Makefile.PL');
    });
    ok ! $result, "Makefile.PL failed";
    like $stderr, qr{Foo/Builder.pm}, "custom builder wasn't found";
  }
  
  # Now make sure it can actually work.
  my $bar_builder;
  stdout_of( sub {
    $bar_builder = Module::Build->subclass( class => 'Bar::Builder' )->new_from_context;
  });
  foreach my $style ('passthrough', 'small') {
    Module::Build::Compat->create_makefile_pl($style, $bar_builder);
    ok -e 'Makefile.PL', "$style Makefile.PL created via subclass";
    my $result;
    stdout_of( sub {
      $result = $mb->run_perl_script('Makefile.PL');
    });
    ok $result, "Makefile.PL ran without error";
  }
}

{
  # Make sure various Makefile.PL arguments are supported
  Module::Build::Compat->create_makefile_pl('passthrough', $mb);

  my $libdir = File::Spec->catdir( $cwd, 't', 'libdir' );
  my $result;
  stdout_of( sub {
    $result = $mb->run_perl_script('Makefile.PL', [],
      [
      "LIB=$libdir",
      'TEST_VERBOSE=1',
      'INSTALLDIRS=perl',
      'POLLUTE=1',
      ]
    );
  });
  ok $result, "passthrough Makefile.PL ran with arguments";
  ok -e 'Build.PL', "Build.PL generated";

  my $new_build = Module::Build->resume();
  is $new_build->installdirs, 'core', "installdirs is core";
  is $new_build->verbose, 1, "tests set for verbose";
  is $new_build->install_destination('lib'), $libdir, "custom libdir";
  is $new_build->extra_compiler_flags->[0], '-DPERL_POLLUTE', "PERL_POLLUTE set";

  # Make sure those switches actually had an effect
  my ($ran_ok, $output);
  $output = stdout_of( sub { $ran_ok = $new_build->do_system(@make, 'test') } );
  ok $ran_ok, "make test ran without error";
  $output =~ s/^/# /gm;  # Don't confuse our own test output
  like $output, qr/(?:# ok \d+\s+)+/, 'Should be verbose';

  # Make sure various Makefile arguments are supported
  $output = stdout_of( sub { $ran_ok = $mb->do_system(@make, 'test', 'TEST_VERBOSE=0') } );
  ok $ran_ok, "make test without verbose ran ok";
  $output =~ s/^/# /gm;  # Don't confuse our own test output
  like $output, qr/(?:# .+basic\.+ok\s+(?:[\d.]+\s*m?s\s*)?)# All tests/,
      'Should be non-verbose';

  $mb->delete_filetree($libdir);
  ok ! -e $libdir, "Sample installation directory should be cleaned up";

  stdout_of( sub { $mb->do_system(@make, 'realclean'); } );
  ok ! -e 'Makefile', "Makefile shouldn't exist";

  1 while unlink 'Makefile.PL';
  ok ! -e 'Makefile.PL', "Makefile.PL cleaned up";
}

{ # Make sure tilde-expansion works

  # C<glob> on MSWin32 uses $ENV{HOME} if defined to do tilde-expansion
  local $ENV{HOME} = 'C:/' if $^O =~ /MSWin/ && !exists( $ENV{HOME} );

  Module::Build::Compat->create_makefile_pl('passthrough', $mb);

  stdout_of( sub {
    $mb->run_perl_script('Makefile.PL', [], ['INSTALL_BASE=~/foo']);
  });
  my $b2 = Module::Build->current;
  ok $b2->install_base, "install_base set";
  unlike $b2->install_base, qr/^~/, "Tildes should be expanded";
  
  stdout_of( sub { $mb->do_system(@make, 'realclean'); } );
  ok ! -e 'Makefile', "Makefile shouldn't exist";

  1 while unlink 'Makefile.PL';
  ok ! -e 'Makefile.PL', "Makefile.PL cleaned up";
}

#########################################################

sub test_makefile_types {
  my %opts = @_;
  $opts{requires} ||= {};

  foreach my $type (@makefile_types) {
    # Create M::B instance 
    my $mb;
    stdout_of( sub {
        $mb = Module::Build->new_from_context;
    });
    ok $mb, "Module::Build->new_from_context";

    # Create and test Makefile.PL
    Module::Build::Compat->create_makefile_pl($type, $mb);
    ok -e 'Makefile.PL', "$type Makefile.PL created";
    test_makefile_pl_requires_perl( $opts{requires}{perl} );
    test_makefile_creation($mb);
    test_makefile_prereq_pm( $opts{requires} );
      
    my ($output,$success);
    # Capture output to keep our STDOUT clean
    $output = stdout_of( sub {
      $success = $mb->do_system(@make);
    });
    ok $success, "make ran without error";

    # Can't let 'test' STDOUT go to our STDOUT, or it'll confuse Test::Harness.
    $output = stdout_of( sub {
      $success = $mb->do_system(@make, 'test');
    });
    ok $success, "make test ran without error";
    like uc $output, qr{DONE\.|SUCCESS}, "make test output indicated success";
    
    $output = stdout_of( sub {
      $success = $mb->do_system(@make, 'realclean');
    });
    ok $success, "make realclean ran without error";

    # Try again with some Makefile.PL arguments
    test_makefile_creation($mb, [], 'INSTALLDIRS=vendor', 1);
    
    1 while unlink 'Makefile.PL';
    ok ! -e 'Makefile.PL', "cleaned up Makefile";
  }
}

sub test_makefile_creation {
  my ($build, $preargs, $postargs, $cleanup) = @_;
  
  my ($output, $result);
  # capture output to avoid polluting our test output
  $output = stdout_of( sub {
      $result = $build->run_perl_script('Makefile.PL', $preargs, $postargs);
  });
  my $label = "Makefile.PL ran without error";
  if ( defined $postargs && length $postargs ) {
    $label .= " (postargs: $postargs)";
  }
  ok $result, $label;
  ok -e 'Makefile', "Makefile exists";
  
  if ($cleanup) {
    $output = stdout_of( sub {
      $build->do_system(@make, 'realclean');
    });
    ok ! -e 'Makefile', "Makefile cleaned up";
  }
  else {
    pass '(skipping cleanup)'; # keep test count constant
  }
}

sub test_makefile_prereq_pm {
  my %requires = %{ $_[0] };
  delete $requires{perl}; # until EU::MM supports this
  SKIP: {
    skip 'Makefile not found', 1 unless -e 'Makefile';
    my $prereq_pm = find_makefile_prereq_pm();
    is_deeply $prereq_pm, \%requires,
      "Makefile has correct PREREQ_PM line";
  }
}

sub test_makefile_pl_requires_perl {
  my $perl_version = shift || q{};
  SKIP: {
    skip 'Makefile.PL not found', 1 unless -e 'Makefile.PL';
    my $file_contents = slurp 'Makefile.PL';
    my $found_requires = $file_contents =~ m{^require $perl_version;}ms;
    if (length $perl_version) {
      ok $found_requires, "Makefile.PL has 'require $perl_version;'"
        or diag "Makefile.PL:\n$file_contents";
    }
    else {
      ok ! $found_requires, "Makefile.PL does not require a perl version";
    }
  }
}

# Following subroutine adapted from code in CPAN.pm 
# by Andreas Koenig and A. Speer.
sub find_makefile_prereq_pm {
  my $fh = IO::File->new( 'Makefile', 'r' ) 
    or die "Can't read Makefile: $!";
  my $req = {};
  local($/) = "\n";
  while (<$fh>) {
    # locate PREREQ_PM
    last if /MakeMaker post_initialize section/;
    my($p) = m{^[\#]
      \s+PREREQ_PM\s+=>\s+(.+)
    }x;
    next unless $p;

    # extract modules
    while ( $p =~ m/(?:\s)([\w\:]+)=>(q\[.*?\]|undef),?/g ){
      my($m,$n) = ($1,$2);
      if ($n =~ /^q\[(.*?)\]$/) {
        $n = $1;
      }
      $req->{$m} = $n;
    }
    last;
  }
  return $req;
}

# cleanup
chdir( $cwd ) or die "Can''t chdir to '$cwd': $!";
$dist->remove;

use File::Path;
rmtree( $tmp );
