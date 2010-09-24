#!/usr/bin/perl

use strict;
use Test::More;
use Config;
use DynaLoader;
use ExtUtils::CBuilder;
use attributes;
use overload;

plan tests => 24;

my ($source_file, $obj_file, $lib_file);

require_ok( 'ExtUtils::ParseXS' );
ExtUtils::ParseXS->import('process_file');

chdir 't' or die "Can't chdir to t/, $!";

use Carp; $SIG{__WARN__} = \&Carp::cluck;

#########################

$source_file = 'XSMore.c';

# Try sending to file
ExtUtils::ParseXS->process_file(
	filename => 'XSMore.xs',
	output   => $source_file,
);
ok -e $source_file, "Create an output file";

my $quiet = $ENV{PERL_CORE} && !$ENV{HARNESS_ACTIVE};
my $b = ExtUtils::CBuilder->new(quiet => $quiet);

SKIP: {
  skip "no compiler available", 2
    if ! $b->have_compiler;
  $obj_file = $b->compile( source => $source_file );
  ok $obj_file;
  ok -e $obj_file, "Make sure $obj_file exists";
}

SKIP: {
  skip "no dynamic loading", 5
    if !$b->have_compiler || !$Config{usedl};
  my $module = 'XSMore';
  $lib_file = $b->link( objects => $obj_file, module_name => $module );
  ok $lib_file;
  ok -e $lib_file,  "Make sure $lib_file exists";

  eval{
    package XSMore;
    our $VERSION = 42;
    our $boot_ok;
    DynaLoader::bootstrap_inherit(__PACKAGE__, $VERSION); # VERSIONCHECK disabled

    sub new{ bless {}, shift }
  };
  is $@, '';
  is ExtUtils::ParseXS::errors(), 0, 'ExtUtils::ParseXS::errors()';

  is $XSMore::boot_ok, 100, 'the BOOT keyword';

  ok XSMore::include_ok(), 'the INCLUDE keyword';
  is prototype(\&XSMore::include_ok), "", 'the PROTOTYPES keyword';

  is prototype(\&XSMore::prototype_ssa), '$$@', 'the PROTOTYPE keyword';

  is_deeply [attributes::get(\&XSMore::attr_method)], [qw(method)], 'the ATTRS keyword';
  is prototype(\&XSMore::attr_method), '$;@', 'ATTRS with prototype';

  is XSMore::return_1(), 1, 'the CASE keyword (1)';
  is XSMore::return_2(), 2, 'the CASE keyword (2)';
  is prototype(\&XSMore::return_1), "", 'ALIAS with prototype (1)';
  is prototype(\&XSMore::return_2), "", 'ALIAS with prototype (2)';

  is XSMore::arg_init(200), 200, 'argument init';

  ok overload::Overloaded(XSMore->new), 'the FALLBACK keyword';
  is abs(XSMore->new), 42, 'the OVERLOAD keyword';

  my @a;
  XSMore::hook(\@a);
  is_deeply \@a, [qw(INIT CODE POSTCALL CLEANUP)], 'the INIT & POSTCALL & CLEANUP keywords';

  is_deeply [XSMore::outlist()], [ord('a'), ord('b')], 'the OUTLIST keyword';

  is XSMore::len("foo"), 3, 'the length keyword';

  # Win32 needs to close the DLL before it can unlink it, but unfortunately
  # dl_unload_file was missing on Win32 prior to perl change #24679!
  if ($^O eq 'MSWin32' and defined &DynaLoader::dl_unload_file) {
    for (my $i = 0; $i < @DynaLoader::dl_modules; $i++) {
      if ($DynaLoader::dl_modules[$i] eq $module) {
        DynaLoader::dl_unload_file($DynaLoader::dl_librefs[$i]);
        last;
      }
    }
  }
}

unless ($ENV{PERL_NO_CLEANUP}) {
  for ( $obj_file, $lib_file, $source_file) {
    next unless defined $_;
    1 while unlink $_;
  }
}
