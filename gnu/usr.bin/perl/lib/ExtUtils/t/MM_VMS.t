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
    @Methods = (qw(wraplist
                   rootdir
                   ext
                   guess_name
                   find_perl
                   path
                   maybe_command
                   maybe_command_in_dirs
                   perl_script
                   file_name_is_absolute
                   replace_manpage_separator
                   init_others
                   constants
                   cflags
                   const_cccmd
                   pm_to_blib
                   tool_autosplit
                   tool_xsubpp
                   xsubpp_version
                   tools_other
                   dist
                   c_o
                   xs_c
                   xs_o
                   top_targets
                   dlsyms
                   dynamic_lib
                   dynamic_bs
                   static_lib
                   manifypods
                   processPL
                   installbin
                   subdir_x
                   clean
                   realclean
                   dist_basics
                   dist_core
                   dist_dir
                   dist_test
                   install
                   perldepend
                   makefile
                   test
                   test_via_harness
                   test_via_script
                   makeaperl
                   nicetext
                  ));
}

BEGIN {
    use Test::More;
    if ($^O eq 'VMS') {
        plan( tests => @Methods + 1 );
    }
    else {
        plan( skip_all => "This is not VMS" );
    }
}

use_ok( 'ExtUtils::MM_VMS' );

foreach my $meth (@Methods) {
    can_ok( 'ExtUtils::MM_VMS', $meth);
}
