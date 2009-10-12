### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use CPANPLUS::Backend;
use CPANPLUS::Internals::Constants::Report;

my $send_tests  = 55;
my $query_tests = 8;
my $total_tests = $send_tests + $query_tests;

use Test::More                  'no_plan';
use Module::Load::Conditional   qw[can_load];

use FileHandle;
use Data::Dumper;

use constant NOBODY => 'nobody@xs4all.nl';

my $conf        = gimme_conf();
my $CB          = CPANPLUS::Backend->new( $conf );
my $ModName     = TEST_CONF_MODULE;
my $ModPrereq   = TEST_CONF_PREREQ;

### pick a high number, but not ~0 as possibly ~0 is unsigned, and we cause 
### an overflow, as happens to version.pm 0.7203 among others.
### ANOTHER bug in version.pm, this time for 64bit:
### https://rt.cpan.org/Ticket/Display.html?id=45241
### so just use a 'big number'(tm) and go from there.
my $HighVersion = 1234567890;
my $Mod         = $CB->module_tree($ModName);
my $int_ver     = $CPANPLUS::Internals::VERSION;

### explicitly enable testing if possible ###
$CB->configure_object->set_conf(cpantest =>1) if $ARGV[0];

my $map = {
    all_ok  => {
        buffer  => '',              # output from build process
        failed  => 0,               # indicate failure
        match   => [qw|/PASS/|],    # list of regexes for the output
        check   => 0,               # check if callbacks got called?
    },
    skipped_test => {
        buffer  => '',
        failed  => 0,
        match   => ['/PASS/',
                    '/tests for this module were skipped during this build/',
                ],
        check   => 0,
        skiptests
                => 1,               # did we skip the tests?
    },                    
    missing_prereq  => {
        buffer  => missing_prereq_buffer(),
        failed  => 1,
        match   => ['/The comments above are created mechanically/',
                    '/computer-generated error report/',
                    '/Below is the error stack from stage/',
                    '/test suite seem to fail without these modules/',
                    '/floo/',
                    '/FAIL/',
                    '/make test/',
                ],
        check   => 1,
    },
    missing_tests   => {
        buffer  => missing_tests_buffer(),
        failed  => 1,
        match   => ['/The comments above are created mechanically/',
                    '/computer-generated error report/',
                    '/Below is the error stack from stage/',
                    '/RECOMMENDATIONS/',
                    '/UNKNOWN/',
                    '/make test/',
                ],
        check   => 0,
    },
    perl_version_too_low_mm => {
        buffer  => perl_version_too_low_buffer_mm(),
        failed  => 1,
        match   => ['/This distribution has been tested/',
                    '/http://testers.cpan.org/',
                    '/NA/',
                ],
        check   => 0,
    },    
    perl_version_too_low_build1 => {
        buffer  => perl_version_too_low_buffer_build(1),
        failed  => 1,
        match   => ['/This distribution has been tested/',
                    '/http://testers.cpan.org/',
                    '/NA/',
                ],
        check   => 0,
    },    
    perl_version_too_low_build2 => {
        buffer  => perl_version_too_low_buffer_build(2),
        failed  => 1,
        match   => ['/This distribution has been tested/',
                    '/http://testers.cpan.org/',
                    '/NA/',
                ],
        check   => 0,
    },    
    prereq_versions_too_low => {
        ### set the prereq version incredibly high
        pre_hook    => sub {
                        my $mod     = shift;
                        my $clone   = $mod->clone;
                        $clone->status->prereqs({ $ModPrereq => $HighVersion });
                        return $clone;
                    },
        failed      => 1,
        match       => ['/This distribution has been tested/',
                        '/http://testers.cpan.org/',
                        '/NA/',
                    ],
        check       => 0,    
    },
    prereq_not_on_cpan => {
        pre_hook    => sub {
                        my $mod     = shift;
                        my $clone   = $mod->clone;
                        $clone->status->prereqs( 
                            { TEST_CONF_INVALID_MODULE, 0 } 
                        );
                        return $clone;
                    },
        failed      => 1,
        match       => ['/This distribution has been tested/',
                        '/http://testers.cpan.org/',
                        '/NA/',
                    ],
        check       => 0,    
    },
    prereq_not_on_cpan_but_core => {
        pre_hook    => sub {
                        my $mod     = shift;
                        my $clone   = $mod->clone;
                        $clone->status->prereqs( 
                            { TEST_CONF_PREREQ, 0 } 
                        );
                        return $clone;
                    },
        failed      => 1,
        match       => ['/This distribution has been tested/',
                        '/http://testers.cpan.org/',
                        '/UNKNOWN/',
                    ],
        check       => 0,    
    },
};

### test config settings 
{   for my $opt ( qw[cpantest cpantest_mx] ) {
        my $warnings;
        local $SIG{__WARN__} = sub { $warnings .= "@_" };

        my $org = $conf->get_conf( $opt );
        ok( $conf->set_conf( $opt => $$ ),
                                "Setting option $opt to $$" );
        is( $conf->get_conf( $opt ), $$,
                                "   Retrieved properly" );
        ok( $conf->set_conf( $opt => $org ),
                                "   Option $opt set back to original" );
        ok( !$warnings,         "   No warnings" );                                
    }
}

### test constants ###
{   {   my $to = CPAN_MAIL_ACCOUNT->('foo');
        is( $to, 'foo@cpan.org',        "Got proper mail account" );
    }

    {   ok(RELEVANT_TEST_RESULT->($Mod),"Test is relevant" );

        ### test non-relevant tests ###
        my $cp = $Mod->clone;
        $cp->module( $Mod->module . '::' . ($^O eq 'beos' ? 'MSDOS' : 'Be') );
        ok(!RELEVANT_TEST_RESULT->($cp),"Test is irrelevant");
    }

    {   my $support = "it works!";
        my @support = ( "No support for OS",
                        "OS unsupported",
                        "os unsupported",
        );
        ok(!UNSUPPORTED_OS->($support), "OS supported");
        ok( UNSUPPORTED_OS->($_),   "OS not supported") for(@support);
    }

    {   ok(PERL_VERSION_TOO_LOW->( perl_version_too_low_buffer_mm() ),
                                        "Perl version too low" );
        ok(PERL_VERSION_TOO_LOW->( perl_version_too_low_buffer_build(1) ),
                                        "Perl version too low" );
        ok(PERL_VERSION_TOO_LOW->( perl_version_too_low_buffer_build(2) ),
                                        "Perl version too low" );
        ok(!PERL_VERSION_TOO_LOW->('foo'),
                                        "   Perl version adequate" );
    }

    {   my $tests = "test.pl";
        my @none  = (   "No tests defined for Foo extension.",
                        "'No tests defined for Foo::Bar extension.'",
                        "'No tests defined.'",
        );
        ok(!NO_TESTS_DEFINED->($tests), "Tests defined");
        ok( NO_TESTS_DEFINED->($_),  "No tests defined")    for(@none);
    }

    {   my $fail = 'MAKE TEST'; my $unknown = 'foo';
        is( TEST_FAIL_STAGE->($fail), lc $fail,
                                        "Proper test fail stage found" );
        is( TEST_FAIL_STAGE->($unknown), 'fetch',
                                        "Proper test fail stage found" );
    }

    ### test missing prereqs        
    {   my $str = q[Can't locate Foo/Bar.pm in @INC];
    
        ### standard test
        {   my @list = MISSING_PREREQS_LIST->( $str );
            is( scalar(@list),  1,      "   List of missing prereqs found" );
            is( $list[0], 'Foo::Bar',   "       Proper prereq found" );
        }
    
        ### multiple mentions of same prereq
        {   my @list = MISSING_PREREQS_LIST->( $str . $str );

            is( scalar(@list),  1,      "   1 result for multiple mentions" );
            is( $list[0], 'Foo::Bar',   "   Proper prereq found" );
        }
    }

    {                                       # cp version, author
        my $header = REPORT_MESSAGE_HEADER->($int_ver,'foo');
        ok( $header,                    "Test header generated" );
        like( $header, qr/Dear foo,/,   "   Proper content found" );
        like( $header, qr/puter-gen/,   "   Proper content found" );
        like( $header, qr/CPANPLUS,/,   "   Proper content found" );
        like( $header, qr/ments may/,   "   Proper content found" );
    }

    {                                       # stage, buffer
        my $header = REPORT_MESSAGE_FAIL_HEADER->('test','buffer');
        ok( $header,                    "Test header generated" );
        like( $header, qr/uploading/,   "   Proper content found" );
        like( $header, qr/RESULTS:/,    "   Proper content found" );
        like( $header, qr/stack/,       "   Proper content found" );
        like( $header, qr/buffer/,      "   Proper content found" );
    }

    {   my $prereqs = REPORT_MISSING_PREREQS->('foo','bar@example.com','Foo::Bar');
        ok( $prereqs,                   "Test output generated" );
        like( $prereqs, qr/'foo \(bar\@example\.com\)'/, 
                                        "   Proper content found" );
        like( $prereqs, qr/Foo::Bar/,   "   Proper content found" );
        like( $prereqs, qr/prerequisi/, "   Proper content found" );
        like( $prereqs, qr/PREREQ_PM/,  "   Proper content found" );
    }

    {   my $prereqs = REPORT_MISSING_PREREQS->(undef,undef,'Foo::Bar');    
        ok( $prereqs,                   "Test output generated" );
        like( $prereqs, qr/Your Name/,  "   Proper content found" );
        like( $prereqs, qr/Foo::Bar/,   "   Proper content found" );
        like( $prereqs, qr/prerequisi/, "   Proper content found" );
        like( $prereqs, qr/PREREQ_PM/,  "   Proper content found" );
    }

    {   my $missing = REPORT_MISSING_TESTS->();
        ok( $missing,                   "Missing test string generated" );
        like( $missing, qr/tests/,      "   Proper content found" );
        like( $missing, qr/Test::More/, "   Proper content found" );
    }

    {   my $missing = REPORT_MESSAGE_FOOTER->();
        ok( $missing,                   "Message footer string generated" );
        like( $missing, qr/NOTE/,       "   Proper content found" );
        like( $missing, qr/identical/,  "   Proper content found" );
        like( $missing, qr/mistaken/,   "   Proper content found" );
        like( $missing, qr/appreciate/, "   Proper content found" );
        like( $missing, qr/Additional/, "   Proper content found" );
    }

    {   my @libs = MISSING_EXTLIBS_LIST->("No library found for -lfoo\nNo library found for -lbar");
        ok( @libs,                      "Missing external libraries found" );
        my @list = qw(foo bar);
        is_deeply( \@libs, \@list,      "   Proper content found" );
    }
    
    {   my $clone   = $Mod->clone;

        my $prereqs = { $ModPrereq => $HighVersion };
    
        $clone->status->prereqs( $prereqs );

        my $str = REPORT_LOADED_PREREQS->( $clone );
        
        like($str, qr/PREREQUISITES:/,  "Listed loaded prerequisites" );
        like($str, qr/\! $ModPrereq\s+\S+\s+\S+/,
                                        "   Proper content found" );
    }
}

### callback tests
{   ### as reported in bug 13086, this callback returned the wrong item 
    ### from the list:
    ### $self->_callbacks->munge_test_report->($Mod, $message, $grade);     
    my $rv = $CB->_callbacks->munge_test_report->( 1..4 );   
    is( $rv, 2,                 "Default 'munge_test_report' callback OK" );
}


### test creating test reports ###
SKIP: {
	skip "You have chosen not to enable test reporting", $total_tests,
        unless $CB->configure_object->get_conf('cpantest');

    skip "No report send & query modules installed", $total_tests
        unless $CB->_have_query_report_modules(verbose => 0);


    SKIP: {   
        my $mod = $CB->module_tree( TEST_CONF_PREREQ ); # is released to CPAN
        ok( $mod,                           "Module retrieved" );
        
        ### so we're not pinned down to this specific version of perl
        my @list = $mod->fetch_report( all_versions => 1 );
        skip "Possibly no net connection, or server down", 7 unless @list;
     
        my $href = $list[0];
        ok( scalar(@list),                  "Fetched test report" );
        is( ref $href, ref {},              "   Return value has hashrefs" );

        ok( $href->{grade},                 "   Has a grade" );

        ### XXX use constants for grades?
        like( $href->{grade}, qr/pass|fail|unknown|na/i,
                                            "   Grade as expected" );

        my $pkg_name = $mod->package_name;
        ok( $href->{dist},                  "   Has a dist" );
        like( $href->{dist}, qr/$pkg_name/, "   Dist as expected" );

        ok( $href->{platform},              "   Has a platform" );
    }

    skip "No report sending modules installed", $send_tests
        unless $CB->_have_send_report_modules(verbose => 0);

    for my $type ( keys %$map ) {


        ### never enter the editor for test reports
        ### but check if the callback actually gets called;
        my $called_edit; my $called_send;
        $CB->_register_callback(
            name => 'edit_test_report',
            code => sub { $called_edit++; 0 }
        );

        $CB->_register_callback(
            name => 'send_test_report',
            code => sub { $called_send++; 1 }
        );

		### reset from earlier tests
		$CB->_register_callback(
            name => 'munge_test_report',
            code => sub { return $_[1] }
        );

        my $mod = $map->{$type}->{'pre_hook'}
                    ? $map->{$type}->{'pre_hook'}->( $Mod )
                    : $Mod;

        my $file = do {
            ### so T::R does not try to resolve our maildomain, which can 
            ### lead to large timeouts for *every* invocation in T::R < 1.51_01
            ### see: http://code.google.com/p/test-reporter/issues/detail?id=15
            local $ENV{MAILDOMAIN} ||= 'example.com';
            $CB->_send_report(
                        module        => $mod,
                        buffer        => $map->{$type}{'buffer'},
                        failed        => $map->{$type}{'failed'},
                        tests_skipped => ($map->{$type}{'skiptests'} ? 1 : 0),
                        save          => 1,
                    );
        };

        ok( $file,              "Type '$type' written to file" );
        ok( -e $file,           "   File exists" );

        my $fh = FileHandle->new($file);
        ok( $fh,                "   Opened file for reading" );

        my $in = do { local $/; <$fh> };
        ok( $in,                "   File has contents" );

        for my $regex ( @{$map->{$type}->{match}} ) {
            like( $in, $regex,  "   File contains expected contents" );
        }

        ### check if our registered callback got called ###
        if( $map->{$type}->{check} ) {
            ok( $called_edit,   "   Callback to edit was called" );
            ok( $called_send,   "   Callback to send was called" );
        }

        #unlink $file;


### T::R tests don't even try to mail, let's not try and be smarter
### ourselves
#        {   ### use a dummy 'editor' and see if the editor
#            ### invocation doesn't break things
#            $conf->set_program( editor => "$^X -le1" );
#            $CB->_callbacks->edit_test_report( sub { 1 } );
#
#            ### XXX whitebox test!!! Might change =/
#            ### this makes test::reporter not ask for what editor to use
#            ### XXX stupid lousy perl warnings;
#            local $Test::Reporter::MacApp = 1;
#            local $Test::Reporter::MacApp = 1;
#
#            ### now try and mail the report to a /dev/null'd mailbox
#            my $ok = $CB->_send_report(
#                            module  => $Mod,
#                            buffer  => $map->{$type}->{'buffer'},
#                            failed  => $map->{$type}->{'failed'},
#                            address => NOBODY,
#                        );
#            ok( $ok,                "   Mailed report to NOBODY" );
#       }
    }
}


sub missing_prereq_buffer {
    return q[
MAKE TEST:
Can't locate floo.pm in @INC (@INC contains: /Users/kane/sources/p4/other/archive-extract/lib /Users/kane/sources/p4/other/file-fetch/lib /Users/kane/sources/p4/other/archive-tar-new/lib /Users/kane/sources/p4/other/carp-trace/lib /Users/kane/sources/p4/other/log-message/lib /Users/kane/sources/p4/other/module-load/lib /Users/kane/sources/p4/other/params-check/lib /Users/kane/sources/p4/other/qmail-checkpassword/lib /Users/kane/sources/p4/other/module-load-conditional/lib /Users/kane/sources/p4/other/term-ui/lib /Users/kane/sources/p4/other/ipc-cmd/lib /Users/kane/sources/p4/other/config-auto/lib /Users/kane/sources/NSA /Users/kane/sources/NSA/misc /Users/kane/sources/NSA/test /Users/kane/sources/beheer/perl /opt/lib/perl5/5.8.3/darwin-2level /opt/lib/perl5/5.8.3 /opt/lib/perl5/site_perl/5.8.3/darwin-2level /opt/lib/perl5/site_perl/5.8.3 /opt/lib/perl5/site_perl .).
BEGIN failed--compilation aborted.
    ];
}

sub missing_tests_buffer {
    return q[
cp lib/Acme/POE/Knee.pm blib/lib/Acme/POE/Knee.pm
cp demo_race.pl blib/lib/Acme/POE/demo_race.pl
cp demo_simple.pl blib/lib/Acme/POE/demo_simple.pl
MAKE TEST:
No tests defined for Acme::POE::Knee extension.
    ];
}

sub perl_version_too_low_buffer_mm {
    return q[
Running [/usr/bin/perl5.8.1 Makefile.PL ]...
Perl v5.8.3 required--this is only v5.8.1, stopped at Makefile.PL line 1.
BEGIN failed--compilation aborted at Makefile.PL line 1.
[ERROR] Could not run '/usr/bin/perl5.8.1 Makefile.PL': Perl v5.8.3 required--this is only v5.8.1, stopped at Makefile.PL line 1.
BEGIN failed--compilation aborted at Makefile.PL line 1.
 -- cannot continue
    ];
}    

sub perl_version_too_low_buffer_build {
    my $type = shift;
    return q[
ERROR: perl: Version 5.006001 is installed, but we need version >= 5.008001
ERROR: version: Prerequisite version isn't installed
ERRORS/WARNINGS FOUND IN PREREQUISITES.  You may wish to install the versions
 of the modules indicated above before proceeding with this installation.
    ]   if($type == 1);
    return q[
ERROR: Version 5.006001 of perl is installed, but we need version >= 5.008001
ERROR: version: Prerequisite version isn't installed
ERRORS/WARNINGS FOUND IN PREREQUISITES.  You may wish to install the versions
 of the modules indicated above before proceeding with this installation.
    ]   if($type == 2);
}    

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
