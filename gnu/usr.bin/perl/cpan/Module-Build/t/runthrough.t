#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 29;

blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

#########################

my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new();
$dist->change_build_pl
({
  module_name => 'Simple',
  scripts     => [ 'script' ],
  license     => 'perl',
  requires    => { 'File::Spec' => 0 },
});

$dist->add_file( 'MANIFEST.SKIP', <<'---' );
^MYMETA.yml$
---
$dist->add_file( 'script', <<'---' );
#!perl -w
print "Hello, World!\n";
---
$dist->add_file( 'lib/Simple/Script.PL', <<'---' );
#!perl -w

my $filename = shift;
open FH, "> $filename" or die "Can't create $filename: $!";
print FH "Contents: $filename\n";
close FH;
---
$dist->regen;

$dist->chdir_in;


#########################

my $mb = Module::Build->new_from_context;
ok $mb;
is $mb->license, 'perl';

# Make sure cleanup files added before create_build_script() get respected
$mb->add_to_cleanup('before_script');

eval {$mb->create_build_script};
is $@, '';
ok -e $mb->build_script;

my $dist_dir = 'Simple-0.01';

# VMS in traditional mode needs the $dist_dir name to not have a '.' in it
# as this is a directory delimiter.  In extended character set mode the dot
# is permitted for Unix format file specifications.
if ($^O eq 'VMS') {
    my $Is_VMS_noefs = 1;
    my $vms_efs = 0;
    if (eval 'require VMS::Feature') {
        $vms_efs = VMS::Feature::current("efs_charset");
    } else {
        my $efs_charset = $ENV{'DECC$EFS_CHARSET'} || '';
        $vms_efs = $efs_charset =~ /^[ET1]/i;
    }
    $Is_VMS_noefs = 0 if $vms_efs;
    if ($Is_VMS_noefs) {
        $dist_dir = 'Simple-0_01';
    }
}

is $mb->dist_dir, $dist_dir;

# The 'cleanup' file doesn't exist yet
ok grep {$_ eq 'before_script'} $mb->cleanup;

$mb->add_to_cleanup('save_out');

# The 'cleanup' file now exists
ok grep {$_ eq 'before_script'} $mb->cleanup;
ok grep {$_ eq 'save_out'     } $mb->cleanup;

{
  # Make sure verbose=>1 works
  my $all_ok = 1;
  my $output = eval {
    stdout_of( sub { $mb->dispatch('test', verbose => 1) } )
  };
  $all_ok &&= is($@, '');
  $all_ok &&= like($output, qr/all tests successful/i);

  # This is the output of lib/Simple/Script.PL
  $all_ok &&= ok(-e $mb->localize_file_path('lib/Simple/Script'));

  unless ($all_ok) {
    # We use diag() so Test::Harness doesn't get confused.
    diag("vvvvvvvvvvvvvvvvvvvvv Simple/t/basic.t output vvvvvvvvvvvvvvvvvvvvv");
    diag($output);
    diag("^^^^^^^^^^^^^^^^^^^^^ Simple/t/basic.t output ^^^^^^^^^^^^^^^^^^^^^");
  }
}

{
  my $output = eval {
    stdout_stderr_of( sub { $mb->dispatch('disttest') } )
  };
  is $@, '';

  # After a test, the distdir should contain a blib/ directory
  ok -e File::Spec->catdir('Simple-0.01', 'blib');

  stdout_stderr_of ( sub { eval {$mb->dispatch('distdir')} } );
  is $@, '';

  # The 'distdir' should contain a lib/ directory
  ok -e File::Spec->catdir('Simple-0.01', 'lib');

  # The freshly run 'distdir' should never contain a blib/ directory, or
  # else it could get into the tarball
  ok ! -e File::Spec->catdir('Simple-0.01', 'blib');

  # Make sure all of the above was done by the new version of Module::Build
  open(my $fh, '<', File::Spec->catfile($dist->dirname, 'META.yml'));
  my $contents = do {local $/; <$fh>};
  $contents =~ /Module::Build version ([0-9_.]+)/m;
  cmp_ok $1, '==', $mb->VERSION, "Check version used to create META.yml: $1 == " . $mb->VERSION;

  SKIP: {
    skip( "Archive::Tar 1.08+ not installed", 1 )
      unless eval { require Archive::Tar && Archive::Tar->VERSION(1.08); 1 };
    $mb->add_to_cleanup($mb->dist_dir . ".tar.gz");
    eval {$mb->dispatch('dist')};
    is $@, '';
  }

}

{
  # Make sure the 'script' file was recognized as a script.
  my $scripts = $mb->script_files;
  ok $scripts->{script};

  # Check that a shebang line is rewritten
  my $blib_script = File::Spec->catfile( qw( blib script script ) );
  ok -e $blib_script;

 SKIP: {
    skip("We do not rewrite shebang on VMS", 1) if $^O eq 'VMS';
    open(my $fh, '<', $blib_script);
    my $first_line = <$fh>;
    isnt $first_line, "#!perl -w\n", "should rewrite the shebang line";
  }
}


eval {$mb->dispatch('realclean')};
is $@, '';

ok ! -e $mb->build_script;
ok ! -e $mb->config_dir;
ok ! -e $mb->dist_dir;

SKIP: {
  skip( 'Windows-only test', 4 ) unless $^O =~ /^MSWin/;

  my $script_data = <<'---';
@echo off
echo Hello, World!
---

  $dist = DistGen->new();
  $dist->change_build_pl({
			  module_name => 'Simple',
			  scripts     => [ 'bin/script.bat' ],
			  license     => 'perl',
			 });

  $dist->add_file( 'bin/script.bat', $script_data );

  $dist->regen;
  $dist->chdir_in;

  $mb = Module::Build->new_from_context;
  ok $mb;

  eval{ $mb->dispatch('build') };
  is $@, '';

  my $script_file = File::Spec->catfile( qw(blib script), 'script.bat' );
  ok -f $script_file, "Native batch file copied to 'scripts'";

  my $out = slurp( $script_file );
  is $out, $script_data, '  unmodified by pl2bat';

}

