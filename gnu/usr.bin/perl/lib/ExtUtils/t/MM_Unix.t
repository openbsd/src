#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

BEGIN { 
    use Test::More; 

    if( $^O =~ /^VMS|os2|MacOS|MSWin32|cygwin|beos|netware$/i ) {
        plan skip_all => 'Non-Unix platform';
    }
    else {
        plan tests => 112;
    }
}

BEGIN { use_ok( 'ExtUtils::MM_Unix' ); }

use vars qw($VERSION);
$VERSION = '0.02';
use strict;
use File::Spec;

my $class = 'ExtUtils::MM_Unix';

# only one of the following can be true
# test should be removed if MM_Unix ever stops handling other OS than Unix
my $os =  ($ExtUtils::MM_Unix::Is_OS2 	|| 0)
	+ ($ExtUtils::MM_Unix::Is_Mac 	|| 0)
	+ ($ExtUtils::MM_Unix::Is_Win32 || 0) 
	+ ($ExtUtils::MM_Unix::Is_Dos 	|| 0)
	+ ($ExtUtils::MM_Unix::Is_VMS   || 0); 
ok ( $os <= 1,  'There can be only one (or none)');

cmp_ok ($ExtUtils::MM_Unix::VERSION, '>=', '1.12606', 'Should be at least version 1.12606');

# when the following calls like canonpath, catdir etc are replaced by
# File::Spec calls, the test's become a bit pointless

foreach ( qw( xx/ ./xx/ xx/././xx xx///xx) )
  {
  is ($class->canonpath($_), File::Spec->canonpath($_), "canonpath $_");
  }

is ($class->catdir('xx','xx'), File::Spec->catdir('xx','xx'),
     'catdir(xx, xx) => xx/xx');
is ($class->catfile('xx','xx','yy'), File::Spec->catfile('xx','xx','yy'),
     'catfile(xx, xx) => xx/xx');

is ($class->file_name_is_absolute('Bombdadil'), 
    File::Spec->file_name_is_absolute('Bombdadil'),
     'file_name_is_absolute()');

is ($class->path(), File::Spec->path(), 'path() same as File::Spec->path()');

foreach (qw/updir curdir rootdir/)
  {
  is ($class->$_(), File::Spec->$_(), $_ );
  }

foreach ( qw /
  c_o
  clean
  const_cccmd
  const_config
  const_loadlibs
  constants
  depend
  dir_target
  dist
  dist_basics
  dist_ci
  dist_core
  dist_dir
  dist_test
  dlsyms
  dynamic
  dynamic_bs
  dynamic_lib
  exescan
  export_list
  extliblist
  find_perl
  fixin
  force
  guess_name
  init_dirscan
  init_main
  init_others
  install
  installbin
  linkext
  lsdir
  macro
  makeaperl
  makefile
  manifypods
  maybe_command_in_dirs
  needs_linking
  pasthru
  perldepend
  pm_to_blib
  ppd
  prefixify
  processPL
  quote_paren
  realclean
  static
  static_lib
  staticmake
  subdir_x
  subdirs
  test
  test_via_harness
  test_via_script
  tool_autosplit
  tool_xsubpp
  tools_other
  top_targets
  writedoc
  xs_c
  xs_cpp
  xs_o
  xsubpp_version 
  / )
  {
      can_ok($class, $_);
  }

###############################################################################
# some more detailed tests for the methods above

ok ( join (' ', $class->dist_basics()), 'distclean :: realclean distcheck');

###############################################################################
# has_link_code tests

my $t = bless { NAME => "Foo" }, $class;
$t->{HAS_LINK_CODE} = 1; 
is ($t->has_link_code(),1,'has_link_code'); is ($t->{HAS_LINK_CODE},1);

$t->{HAS_LINK_CODE} = 0;
is ($t->has_link_code(),0); is ($t->{HAS_LINK_CODE},0);

delete $t->{HAS_LINK_CODE}; delete $t->{OBJECT};
is ($t->has_link_code(),0); is ($t->{HAS_LINK_CODE},0);

delete $t->{HAS_LINK_CODE}; $t->{OBJECT} = 1;
is ($t->has_link_code(),1); is ($t->{HAS_LINK_CODE},1);

delete $t->{HAS_LINK_CODE}; delete $t->{OBJECT}; $t->{MYEXTLIB} = 1;
is ($t->has_link_code(),1); is ($t->{HAS_LINK_CODE},1);

delete $t->{HAS_LINK_CODE}; delete $t->{MYEXTLIB}; $t->{C} = [ 'Gloin' ];
is ($t->has_link_code(),1); is ($t->{HAS_LINK_CODE},1);

###############################################################################
# libscan

is ($t->libscan('RCS'),'','libscan on RCS');
is ($t->libscan('CVS'),'','libscan on CVS');
is ($t->libscan('SCCS'),'','libscan on SCCS');
is ($t->libscan('Fatty'),'Fatty','libscan on something not RCS, CVS or SCCS');

###############################################################################
# maybe_command

is ($t->maybe_command('blargel'),undef,"'blargel' isn't a command");

###############################################################################
# nicetext (dummy method)

is ($t->nicetext('LOTR'),'LOTR','nicetext');

###############################################################################
# parse_version

my $self_name = $ENV{PERL_CORE} ? '../lib/ExtUtils/t/MM_Unix.t' 
                                : 'MM_Unix.t';

is( $t->parse_version($self_name), '0.02',  'parse_version on ourself');

my %versions = (
                '$VERSION = 0.0'    => 0.0,
                '$VERSION = -1.0'   => -1.0,
                '$VERSION = undef'  => 'undef',
                '$wibble  = 1.0'    => 'undef',
               );

while( my($code, $expect) = each %versions ) {
    open(FILE, ">VERSION.tmp") || die $!;
    print FILE "$code\n";
    close FILE;

    is( $t->parse_version('VERSION.tmp'), $expect, $code );

    unlink "VERSION.tmp";
}


###############################################################################
# perl_script (on unix any ordinary, readable file)

is ($t->perl_script($self_name),$self_name, 'we pass as a perl_script()');

###############################################################################
# perm_rw perm_rwx

is ($t->perm_rw(),'644', 'perm_rw() is 644');
is ($t->perm_rwx(),'755', 'perm_rwx() is 755');

###############################################################################
# post_constants, postamble, post_initialize

foreach (qw/ post_constants postamble post_initialize/)
  {
  is ($t->$_(),'', "$_() is an empty string");
  }

###############################################################################
# replace_manpage_separator 

is ($t->replace_manpage_separator('Foo/Bar'),'Foo::Bar','manpage_separator'); 

###############################################################################
# export_list, perl_archive, perl_archive_after

foreach (qw/ export_list perl_archive perl_archive_after/)
  {
  is ($t->$_(),'',"$_() is empty string on Unix"); 
  }


{
    $t->{CCFLAGS} = '-DMY_THING';
    $t->{LIBPERL_A} = 'libperl.a';
    $t->{LIB_EXT}   = '.a';
    local $t->{NEEDS_LINKING} = 1;
    $t->cflags();

    # Brief bug where CCFLAGS was being blown away
    is( $t->{CCFLAGS}, '-DMY_THING',    'cflags retains CCFLAGS' );
}

