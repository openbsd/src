#!/usr/bin/perl -w

use strict;
use Test::More;
use File::Spec;
local $|=1;

my @platforms = qw(Cygwin Epoc Mac OS2 Unix VMS Win32);
my $tests_per_platform = 7;

plan tests => 1 + @platforms * $tests_per_platform;

my %volumes = (
	       Mac => 'Macintosh HD',
	       OS2 => 'A:',
	       Win32 => 'A:',
	       VMS => 'v',
	      );
my %other_vols = (
		  Mac => 'Mounted Volume',
		  OS2 => 'B:',
		  Win32 => 'B:',
		  VMS => 'w',
	      );

ok 1, "Loaded";

foreach my $platform (@platforms) {
  my $module = "File::Spec::$platform";
  
 SKIP:
  {
    eval "require $module; 1";

    skip "Can't load $module", $tests_per_platform
      if $@;
    
    my $v = $volumes{$platform} || '';
    my $other_v = $other_vols{$platform} || '';
    
    # Fake out the rootdir on MacOS
    no strict 'refs';
    my $save_w = $^W;
    $^W = 0;
    local *{"File::Spec::Mac::rootdir"} = sub { "Macintosh HD:" };
    $^W = $save_w;
    use strict 'refs';
    
    my ($file, $base, $result);

    $base = $module->catpath($v, $module->catdir('', 'foo'), '');
    $base = $module->catdir($module->rootdir, 'foo');

    is $module->file_name_is_absolute($base), 1, "$base is absolute on $platform";


    # abs2rel('A:/foo/bar', 'A:/foo')    ->  'bar'
    $file = $module->catpath($v, $module->catdir($module->rootdir, 'foo', 'bar'), 'file');
    $base = $module->catpath($v, $module->catdir($module->rootdir, 'foo'), '');
    $result = $module->catfile('bar', 'file');
    is $module->abs2rel($file, $base), $result, "$platform->abs2rel($file, $base)";
    
    # abs2rel('A:/foo/bar', 'B:/foo')    ->  'A:/foo/bar'
    $base = $module->catpath($other_v, $module->catdir($module->rootdir, 'foo'), '');
    $result = volumes_differ($module, $file, $base) ? $file : $module->catfile('bar', 'file');
    is $module->abs2rel($file, $base), $result, "$platform->abs2rel($file, $base)";

    # abs2rel('A:/foo/bar', '/foo')      ->  'A:/foo/bar'
    $base = $module->catpath('', $module->catdir($module->rootdir, 'foo'), '');
    $result = volumes_differ($module, $file, $base) ? $file : $module->catfile('bar', 'file');
    is $module->abs2rel($file, $base), $result, "$platform->abs2rel($file, $base)";

    # abs2rel('/foo/bar', 'A:/foo')    ->  '/foo/bar'
    $file = $module->catpath('', $module->catdir($module->rootdir, 'foo', 'bar'), 'file');
    $base = $module->catpath($v, $module->catdir($module->rootdir, 'foo'), '');
    $result = volumes_differ($module, $file, $base) ? $file : $module->catfile('bar', 'file');
    is $module->abs2rel($file, $base), $result, "$platform->abs2rel($file, $base)";
    
    # abs2rel('/foo/bar', 'B:/foo')    ->  '/foo/bar'
    $base = $module->catpath($other_v, $module->catdir($module->rootdir, 'foo'), '');
    $result = volumes_differ($module, $file, $base) ? $file : $module->catfile('bar', 'file');
    is $module->abs2rel($file, $base), $result, "$platform->abs2rel($file, $base)";
    
    # abs2rel('/foo/bar', '/foo')      ->  'bar'
    $base = $module->catpath('', $module->catdir($module->rootdir, 'foo'), '');
    $result = $module->catfile('bar', 'file');
    is $module->abs2rel($file, $base), $result, "$platform->abs2rel($file, $base)";
  }
}

sub volumes_differ {
  my ($module, $one, $two) = @_;
  my ($one_v) = $module->splitpath( $module->rel2abs($one) );
  my ($two_v) = $module->splitpath( $module->rel2abs($two) );
  return $one_v ne $two_v;
}
