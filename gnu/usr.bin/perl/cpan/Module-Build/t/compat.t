#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest;
use File::Spec;
use IO::File;
use Config;

# Don't let our own verbosity/test_file get mixed up with our subprocess's
my @makefile_keys = qw(TEST_VERBOSE HARNESS_VERBOSE TEST_FILES MAKEFLAGS);
local  @ENV{@makefile_keys};
delete @ENV{@makefile_keys};

my @makefile_types = qw(small passthrough traditional);
my $tests_per_type = 15;

#find_in_path does not understand VMS.

if ( $Config{make} && $^O ne 'VMS' ? find_in_path($Config{make}) : 1 ) {
    plan 'no_plan';
} else {
    plan skip_all => "Don't know how to invoke 'make'";
}

my $is_vms_mms = ($^O eq 'VMS') && ($Config{make} =~ /MM[SK]/i);

blib_load('Module::Build');
blib_load('Module::Build::Version');


#########################

my $tmp = MBTest->tmpdir;

# Create test distribution; set requires and build_requires
use DistGen;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

$dist->chdir_in;


#########################

blib_load('Module::Build');
blib_load('Module::Build::Compat');

use Carp;  $SIG{__WARN__} = \&Carp::cluck;

my @make = $Config{make} eq 'nmake' ? ('nmake', '-nologo') : ($Config{make});

my $makefile = 'Makefile';

# VMS MMK/MMS by convention use Descrip.MMS
if ($is_vms_mms) {
    $makefile = 'Descrip.MMS';
}


#########################

# Test without requires

test_makefile_types();

# Test with requires and PL_files

my $distname = $dist->name;
$dist->change_build_pl({
  module_name         => $distname,
  license             => 'perl',
  requires            => {
    'perl'        => $],
    'File::Spec'  => 0.2,
  },
  build_requires => {
      'Test::More' => 0,
      'File::Spec' => 0,
  },
  PL_files            => { 'foo.PL' => 'foo' },
});

$dist->add_file("foo.PL", <<'END');
open my $fh, ">$ARGV[0]" or die $!;
print $fh "foo\n";
END

$dist->regen;

test_makefile_types(
    requires => {
        'perl' => $],
        'File::Spec' => 0.2,
    },
    build_requires => {
        'Test::More' => 0,
        'File::Spec' => 0,
    },
    PL_files => {
        'foo.PL' => 'foo',
    },
);

######################

$dist->change_build_pl({
  module_name         => $distname,
  license             => 'perl',
});
$dist->regen;

# Create M::B instance but don't pollute STDOUT
my $mb;
stdout_stderr_of( sub {
    $mb = Module::Build->new_from_context;
});
ok $mb, "Module::Build->new_from_context";


{
  # Make sure fake_makefile() can run without 'build_class', as it may be
  # in older-generated Makefile.PLs
  my $warning = '';
  local $SIG{__WARN__} = sub { $warning = shift; };

  my $maketext = eval { Module::Build::Compat->fake_makefile(makefile => $makefile) };
  is $@, '', "fake_makefile lived";
  like $maketext, qr/^realclean/m, "found 'realclean' in fake_makefile output";
  like $warning, qr/build_class/, "saw warning about 'build_class'";
}

{
  # Make sure custom builder subclass is used in the created
  # Makefile.PL - make sure it fails in the right way here.
  local @Foo::Builder::ISA = qw(Module::Build);
  my $foo_builder;
  stdout_stderr_of( sub {
    $foo_builder = Foo::Builder->new_from_context;
  });
  foreach my $style ('passthrough', 'small') {
    create_makefile_pl($style, $foo_builder);

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
  stdout_stderr_of( sub {
    $bar_builder = Module::Build->subclass( class => 'Bar::Builder' )->new_from_context;
  });
  foreach my $style ('passthrough', 'small') {
    create_makefile_pl($style, $bar_builder);
    my $result;
    stdout_stderr_of( sub {
      $result = $mb->run_perl_script('Makefile.PL');
    });
    ok $result, "Makefile.PL ran without error";
  }
}

{
  # Make sure various Makefile.PL arguments are supported
  create_makefile_pl('passthrough', $mb);

  my $libdir = File::Spec->catdir( $tmp, 'libdir' );
  my $result;
  stdout_stderr_of( sub {
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
  $output = stdout_stderr_of( sub { $ran_ok = $new_build->do_system(@make, 'test') } );
  ok $ran_ok, "make test ran without error";
  $output =~ s/^/# /gm;  # Don't confuse our own test output
  like $output, qr/(?:# ok \d+\s+)+/, 'Should be verbose';

  # Make sure various Makefile arguments are supported
  my $make_macro = 'TEST_VERBOSE=0';

  # VMS MMK/MMS macros use different syntax.
  if ($is_vms_mms) {
    $make_macro = '/macro=("' . $make_macro . '")';
  }

  $output = stdout_stderr_of( sub {
    local $ENV{HARNESS_TIMER}; # RT#39635 - timer messes with output
    $ran_ok = $mb->do_system(@make, 'test', $make_macro)
  } );

  ok $ran_ok, "make test without verbose ran ok";
  $output =~ s/^/# /gm;  # Don't confuse our own test output
  like $output,
       qr/# .+basic(\.t)?[.\s#]+ok[.\s#]+All tests successful/,
       'Should be non-verbose';

  (my $libdir2 = $libdir) =~ s/libdir/lbiidr/;
  my $libarch2 = File::Spec->catdir($libdir2, 'arch');

  SKIP: {
    my @cases = (
      {
        label => "INSTALLDIRS=vendor",
        args => [ 'INSTALLDIRS=vendor', "INSTALLVENDORLIB=$libdir2", "INSTALLVENDORARCH=$libarch2"],
        check => qr/\Q$libdir2\E .* Simple\.pm/ix,
      },
      {
        label => "PREFIX=\$libdir2",
        args => [ "PREFIX=$libdir2"],
        check => qr/\Q$libdir2\E .* Simple\.pm/ix,
      },
      {
        label => "PREFIX=\$libdir2 LIB=mylib",
        args => [ "PREFIX=$libdir2", "LIB=mylib" ],
        check => qr{\Q$libdir2\E[/\\]mylib[/\\]Simple\.pm}ix,
      },
    );

    require ExtUtils::Install;
    skip "Needs ExtUtils::Install 1.32 or later", 2 * @cases
      if ExtUtils::Install->VERSION < 1.32;

    skip "Needs upstream patch at http://rt.cpan.org/Public/Bug/Display.html?id=55288", 2 * @cases
      if $^O eq 'VMS';

    for my $c (@cases) {
      my @make_args = @{$c->{args}};
      if ($is_vms_mms) { # VMS MMK/MMS macros use different syntax.
        $make_args[0] = '/macro=("' . join('","',@make_args) . '")';
        pop @make_args while scalar(@make_args) > 1;
      }
      ($output) = stdout_stderr_of(
        sub {
          $result = $mb->run_perl_script('Makefile.PL', [], \@make_args);
          $ran_ok = $mb->do_system(@make, 'fakeinstall');
        }
      );

      ok $ran_ok, "fakeinstall $c->{label} ran ok";
      $output =~ s/^/# /gm;  # Don't confuse our own test output
      like $output, $c->{check},
          "Saw destination directory for $c->{label}";
    }
  }

  stdout_stderr_of( sub { $mb->do_system(@make, 'realclean'); } );
  ok ! -e $makefile, "$makefile shouldn't exist";

  1 while unlink 'Makefile.PL';
  ok ! -e 'Makefile.PL', "Makefile.PL cleaned up";

  1 while unlink $libdir, $libdir2;
}

{ # Make sure tilde-expansion works

  # C<glob> on MSWin32 uses $ENV{HOME} if defined to do tilde-expansion
  local $ENV{HOME} = 'C:/' if $^O =~ /MSWin/ && !exists( $ENV{HOME} );

  create_makefile_pl('passthrough', $mb);

  stdout_stderr_of( sub {
    $mb->run_perl_script('Makefile.PL', [], ['INSTALL_BASE=~/foo']);
  });
  my $b2 = Module::Build->current;
  ok $b2->install_base, "install_base set";
  unlike $b2->install_base, qr/^~/, "Tildes should be expanded";

  stdout_stderr_of( sub { $mb->do_system(@make, 'realclean'); } );
  ok ! -e $makefile, "$makefile shouldn't exist";

  1 while unlink 'Makefile.PL';
  ok ! -e 'Makefile.PL', "Makefile.PL cleaned up";
}

{
  $dist->add_file('t/deep/foo.t', q{});
  $dist->regen;

  my $mb;
  stdout_stderr_of( sub {
      $mb = Module::Build->new_from_context( recursive_test_files => 1 );
  });

  create_makefile_pl('traditional', $mb);
  my $args = extract_writemakefile_args() || {};

  if ( exists $args->{test}->{TESTS} ) {
    is $args->{test}->{TESTS},
      join( q{ },
        File::Spec->catfile(qw(t *.t)),
        File::Spec->catfile(qw(t deep *.t))
      ),
      'Makefile.PL has correct TESTS line for recursive test files';
  } else {
    ok( ! exists $args->{TESTS}, 'Not using incorrect recursive tests key' );
  }

}

#########################################################

sub _merge_prereqs {
  my ($first, $second) = @_;
  my $new = { %$first };
  for my $k (keys %$second) {
    if ( exists $new->{$k} ) {
      my ($v1,$v2) = ($new->{$k},$second->{$k});
      $new->{$k} = ($v1 > $v2 ? $v1 : $v2);
    }
    else {
      $new->{$k} = $second->{$k};
    }
  }
  return $new;
}

sub test_makefile_types {
  my %opts = @_;
  $opts{requires} ||= {};
  $opts{build_requires} ||= {};
  $opts{PL_files} ||= {};

  foreach my $type (@makefile_types) {
    # Create M::B instance
    my $mb;
    stdout_stderr_of( sub {
        $mb = Module::Build->new_from_context;
    });
    ok $mb, "Module::Build->new_from_context";

    # Create and test Makefile.PL
    create_makefile_pl($type, $mb);

    test_makefile_pl_requires_perl( $opts{requires}{perl} );
    test_makefile_creation($mb);
    test_makefile_prereq_pm( _merge_prereqs($opts{requires}, $opts{build_requires}) );
    test_makefile_pl_files( $opts{PL_files} ) if $type eq 'traditional';

    my ($output,$success);
    # Capture output to keep our STDOUT clean
    $output = stdout_stderr_of( sub {
      $success = $mb->do_system(@make);
    });
    ok $success, "make ran without error";

    for my $file (values %{ $opts{PL_files} }) {
        ok -e $file, "PL_files generated - $file";
    }

    # Can't let 'test' STDOUT go to our STDOUT, or it'll confuse Test::Harness.
    $output = stdout_stderr_of( sub {
      $success = $mb->do_system(@make, 'test');
    });
    ok $success, "make test ran without error";
    like uc $output, qr{DONE\.|SUCCESS}, "make test output indicated success";

    $output = stdout_stderr_of( sub {
      $success = $mb->do_system(@make, 'realclean');
    });
    ok $success, "make realclean ran without error";

    # Try again with some Makefile.PL arguments
    test_makefile_creation($mb, [], 'INSTALLDIRS=vendor', 'realclean');

    # Try again using distclean
    test_makefile_creation($mb, [], '', 'distclean');

    1 while unlink 'Makefile.PL';
    ok ! -e 'Makefile.PL', "cleaned up Makefile";
  }
}

sub test_makefile_creation {
  my ($build, $preargs, $postargs, $cleanup) = @_;

  my ($output, $result);
  # capture output to avoid polluting our test output
  $output = stdout_stderr_of( sub {
      $result = $build->run_perl_script('Makefile.PL', $preargs, $postargs);
  });
  my $label = "Makefile.PL ran without error";
  if ( defined $postargs && length $postargs ) {
    $label .= " (postargs: $postargs)";
  }
  ok $result, $label;
  ok -e $makefile, "$makefile exists";

  if ($cleanup) {
    # default to 'realclean' unless we recognize the clean method
    $cleanup = 'realclean' unless $cleanup =~ /^(dist|real)clean$/;
    my ($stdout, $stderr ) = stdout_stderr_of (sub {
      $build->do_system(@make, $cleanup);
    });
    ok ! -e $makefile, "$makefile cleaned up with $cleanup";
  }
  else {
    pass '(skipping cleanup)'; # keep test count constant
  }
}

sub test_makefile_prereq_pm {
  my %requires = %{ $_[0] };
  delete $requires{perl}; # until EU::MM supports this
  SKIP: {
    skip "$makefile not found", 1 unless -e $makefile;
    my $prereq_pm = find_params_in_makefile()->{PREREQ_PM} || {};
    is_deeply $prereq_pm, \%requires,
      "$makefile has correct PREREQ_PM line";
  }
}

sub test_makefile_pl_files {
  my $expected = shift;

  SKIP: {
    skip 1, 'Makefile.PL not found' unless -e 'Makefile.PL';
    my $args = extract_writemakefile_args() || {};
    is_deeply $args->{PL_FILES}, $expected,
      "Makefile.PL has correct PL_FILES line";
  }
}

sub test_makefile_pl_requires_perl {
  my $perl_version = shift || q{};
  SKIP: {
    skip 1, 'Makefile.PL not found' unless -e 'Makefile.PL';
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

sub find_params_in_makefile {
  my $fh = IO::File->new( $makefile, 'r' )
    or die "Can't read $makefile: $!";
  local($/) = "\n";

  my %params;
  while (<$fh>) {
    # Blank line after params.
    last if keys %params and !/\S+/;

    next unless m{^\# \s+ ( [A-Z_]+ ) \s+ => \s+ ( .* )$}x;

    my($key, $val) = ($1, $2);
    # extract keys and values
    while ( $val =~ m/(?:\s)(\S+)=>(q\[.*?\]|undef),?/g ) {
      my($m,$n) = ($1,$2);
      if ($n =~ /^q\[(.*?)\]$/) {
        $n = $1;
      }
      $params{$key}{$m} = $n;
    }
  }

  return \%params;
}

sub extract_writemakefile_args {
  SKIP: {
    skip 1, 'Makefile.PL not found' unless -e 'Makefile.PL';
    my $file_contents = slurp 'Makefile.PL';
    my ($args) = $file_contents =~ m{^WriteMakefile\n\((.*)\).*;}ms;
    ok $args, "Found WriteMakefile arguments"
        or diag "Makefile.PL:\n$file_contents";
    my %args = eval $args or diag $args; ## no critic
    return \%args;
  }
}

sub create_makefile_pl {
    my @args = @_;
    stdout_stderr_of( sub { Module::Build::Compat->create_makefile_pl(@args) } );
    my $ok = ok -e 'Makefile.PL', "$_[0] Makefile.PL created";

    # Some really conservative make's, like HP/UX, assume files with the same
    # timestamp are out of date.  Send the Makefile.PL one second into the past
    # so its older than the Makefile it will generate.
    # See [rt.cpan.org 45700]
    my $mtime = (stat("Makefile.PL"))[9];
    utime $mtime, $mtime - 1, "Makefile.PL";

    return $ok;
}
