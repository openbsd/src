package CPANPLUS::Internals::Report;

use strict;

use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;
use CPANPLUS::Internals::Constants::Report;

use Data::Dumper;

use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';
use version;

$Params::Check::VERBOSE = 1;

### for the version ###
require CPANPLUS::Internals;

=head1 NAME

CPANPLUS::Internals::Report - internals for sending test reports

=head1 SYNOPSIS

  ### enable test reporting
  $cb->configure_object->set_conf( cpantest => 1 );

  ### set custom mx host, shouldn't normally be needed
  $cb->configure_object->set_conf( cpantest_mx => 'smtp.example.com' );

=head1 DESCRIPTION

This module provides all the functionality to send test reports to
C<http://testers.cpan.org> using the C<Test::Reporter> module.

All methods will be called automatically if you have C<CPANPLUS>
configured to enable test reporting (see the C<SYNOPSIS>).

=head1 METHODS

=head2 $bool = $cb->_have_query_report_modules

This function checks if all the required modules are here for querying
reports. It returns true and loads them if they are, or returns false
otherwise.

=head2 $bool = $cb->_have_send_report_modules

This function checks if all the required modules are here for sending
reports. It returns true and loads them if they are, or returns false
otherwise.

=cut

### XXX remove this list and move it into selfupdate, somehow..
### this is dual administration
{   my $query_list = {
        'File::Fetch'          => '0.13_02',
        'Parse::CPAN::Meta'    => '0.0',
        'File::Temp'           => '0.0',
    };

    my $send_list = {
        %$query_list,
        'Test::Reporter' => '1.54',
    };

    sub _have_query_report_modules {
        my $self = shift;
        my $conf = $self->configure_object;
        my %hash = @_;

        my $tmpl = {
            verbose => { default => $conf->get_conf('verbose') },
        };

        my $args = check( $tmpl, \%hash ) or return;

        return can_load( modules => $query_list, verbose => $args->{verbose} )
                ? 1
                : 0;
    }

    sub _have_send_report_modules {
        my $self = shift;
        my $conf = $self->configure_object;
        my %hash = @_;

        my $tmpl = {
            verbose => { default => $conf->get_conf('verbose') },
        };

        my $args = check( $tmpl, \%hash ) or return;

        return can_load( modules => $send_list, verbose => $args->{verbose} )
                ? 1
                : 0;
    }
}

=head2 @list = $cb->_query_report( module => $modobj, [all_versions => BOOL, verbose => BOOL] )

This function queries the CPAN testers database at
I<http://testers.cpan.org/> for test results of specified module objects,
module names or distributions.

The optional argument C<all_versions> controls whether all versions of
a given distribution should be grabbed.  It defaults to false
(fetching only reports for the current version).

Returns the a list with the following data structures (for CPANPLUS
version 0.042) on success, or false on failure. The contents of the
data structure depends on what I<http://testers.cpan.org> returns,
but generally looks like this:

          {
            'grade' => 'PASS',
            'dist' => 'CPANPLUS-0.042',
            'platform' => 'i686-pld-linux-thread-multi'
            'details' => 'http://nntp.x.perl.org/group/perl.cpan.testers/98316'
            ...
          },
          {
            'grade' => 'PASS',
            'dist' => 'CPANPLUS-0.042',
            'platform' => 'i686-linux-thread-multi'
            'details' => 'http://nntp.x.perl.org/group/perl.cpan.testers/99416'
            ...
          },
          {
            'grade' => 'FAIL',
            'dist' => 'CPANPLUS-0.042',
            'platform' => 'cygwin-multi-64int',
            'details' => 'http://nntp.x.perl.org/group/perl.cpan.testers/99371'
            ...
          },
          {
            'grade' => 'FAIL',
            'dist' => 'CPANPLUS-0.042',
            'platform' => 'i586-linux',
            'details' => 'http://nntp.x.perl.org/group/perl.cpan.testers/99396'
            ...
          },

The status of the test can be one of the following:
UNKNOWN, PASS, FAIL or NA (not applicable).

=cut

sub _query_report {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    my($mod, $verbose, $all);
    my $tmpl = {
        module          => { required => 1, allow => IS_MODOBJ,
                                store => \$mod },
        verbose         => { default => $conf->get_conf('verbose'),
                                store => \$verbose },
        all_versions    => { default => 0, store => \$all },
    };

    check( $tmpl, \%hash ) or return;

    ### check if we have the modules we need for querying
    return unless $self->_have_query_report_modules( verbose => 1 );


    ### XXX no longer use LWP here. However, that means we don't
    ### automagically set proxies anymore!!!
    # my $ua = LWP::UserAgent->new;
    # $ua->agent( CPANPLUS_UA->() );
    #
    ### set proxies if we have them ###
    # $ua->env_proxy();

    my $url = TESTERS_URL->($mod->package_name);
    my $ff  = File::Fetch->new( uri => $url );

    msg( loc("Fetching: '%1'", $url), $verbose );

    my $res = do {
        my $tempdir = File::Temp::tempdir();
        my $where   = $ff->fetch( to => $tempdir );

        unless( $where ) {
            error( loc( "Fetching report for '%1' failed: %2",
                        $url, $ff->error ) );
            return;
        }

        my $fh = OPEN_FILE->( $where );

        do { local $/; <$fh> };
    };

    my ($aref) = eval { Parse::CPAN::Meta::Load( $res ) };

    if( $@ ) {
        error(loc("Error reading result: %1", $@));
        return;
    };

    my $dist    = $mod->package_name .'-'. $mod->package_version;
    my $details = TESTERS_DETAILS_URL->($mod->package_name);

    my @rv;
    for my $href ( @$aref ) {
        next unless $all or defined $href->{'distversion'} &&
                            $href->{'distversion'} eq $dist;

        $href->{'details'}  = $details;

        ### backwards compatibility :(
        $href->{'dist'}     ||= $href->{'distversion'};
        $href->{'grade'}    ||= $href->{'action'} || $href->{'status'};

        push @rv, $href;
    }

    return @rv if @rv;
    return;
}

=pod

=head2 $bool = $cb->_send_report( module => $modobj, buffer => $make_output, failed => BOOL, [save => BOOL, address => $email_to, verbose => BOOL, force => BOOL]);

This function sends a testers report to C<cpan-testers@perl.org> for a
particular distribution.
It returns true on success, and false on failure.

It takes the following options:

=over 4

=item module

The module object of this particular distribution

=item buffer

The output buffer from the 'make/make test' process

=item failed

Boolean indicating if the 'make/make test' went wrong

=item save

Boolean indicating if the report should be saved locally instead of
mailed out. If provided, this function will return the location the
report was saved to, rather than a simple boolean 'TRUE'.

Defaults to false.

=item address

The email address to mail the report for. You should never need to
override this, but it might be useful for debugging purposes.

Defaults to C<cpan-testers@perl.org>.

=item verbose

Boolean indicating on whether or not to be verbose.

Defaults to your configuration settings

=item force

Boolean indicating whether to force the sending, even if the max
amount of reports for fails have already been reached, or if you
may already have sent it before.

Defaults to your configuration settings

=back

=cut

sub _send_report {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    ### do you even /have/ test::reporter? ###
    unless( $self->_have_send_report_modules(verbose => 1) ) {
        error( loc( "You don't have '%1' (or modules required by '%2') ".
                    "installed, you cannot report test results.",
                    'Test::Reporter', 'Test::Reporter' ) );
        return;
    }

    ### check arguments ###
    my ($buffer, $failed, $mod, $verbose, $force, $address, $save,
        $tests_skipped, $status );
    my $tmpl = {
            module  => { required => 1, store => \$mod, allow => IS_MODOBJ },
            buffer  => { required => 1, store => \$buffer },
            failed  => { required => 1, store => \$failed },
            status  => { default => {}, store => \$status, strict_type => 1 },
            address => { default  => CPAN_TESTERS_EMAIL, store => \$address },
            save    => { default  => 0, store => \$save },
            verbose => { default  => $conf->get_conf('verbose'),
                            store => \$verbose },
            force   => { default  => $conf->get_conf('force'),
                            store => \$force },
            tests_skipped
                    => { default => 0, store => \$tests_skipped },
    };

    check( $tmpl, \%hash ) or return;

    ### get the data to fill the email with ###
    my $name    = $mod->module;
    my $dist    = $mod->package_name . '-' . $mod->package_version;
    my $author  = $mod->author->author;
    my $distfile= $mod->author->cpanid . "/" . $mod->package;
    my $email   = $mod->author->email || CPAN_MAIL_ACCOUNT->( $author );
    my $cp_conf = $conf->get_conf('cpantest') || '';
    my $int_ver = $CPANPLUS::Internals::VERSION;
    my $cb      = $mod->parent;


    ### will be 'fetch', 'make', 'test', 'install', etc ###
    my $stage   = TEST_FAIL_STAGE->($buffer);

    ### determine the grade now ###

    my $grade;
    ### check if this is a platform specific module ###
    ### if we failed the test, there may be reasons why
    ### an 'NA' might have to be instead
    GRADE: { if ( $failed ) {


        ### XXX duplicated logic between this block
        ### and REPORTED_LOADED_PREREQS :(

        ### figure out if the prereqs are on CPAN at all
        ### -- if not, send NA grade
        ### Also, if our version of prereqs is too low,
        ### -- send NA grade.
        ### This is to address bug: #25327: do not count
        ### as FAIL modules where prereqs are not filled
        {   my $prq = $mod->status->prereqs || {};

            PREREQ: while( my($prq_name,$prq_ver) = each %$prq ) {

                # 'perl' listed as prereq

                if ( $prq_name eq 'perl' ) {
                   my $req_ver = eval { version->new( $prq_ver ) };
                   next PREREQ unless $req_ver;
                   if ( version->new( $] ) < $req_ver ) {
                      msg(loc("'%1' requires a higher version of perl than your current ".
                              "version -- sending N/A grade.", $name), $verbose);

                      $grade = GRADE_NA;
                      last GRADE;
                   }
                   next PREREQ;
                }

                my $obj = $cb->module_tree( $prq_name );
                my $sub = CPANPLUS::Module->can(
                            'module_is_supplied_with_perl_core' );

                ### if we can't find the module and it's not supplied with core.
                ### this addresses: #32064: NA reports generated for failing
                ### tests where core prereqs are specified
                ### Note that due to a bug in Module::CoreList, in some released
                ### version of perl (5.8.6+ and 5.9.2-4 at the time of writing)
                ### 'Config' is not recognized as a core module. See this bug:
                ###    http://rt.cpan.org/Ticket/Display.html?id=32155
                if( !$obj and !defined $sub->( $prq_name ) ) {
                    msg(loc( "Prerequisite '%1' for '%2' could not be obtained".
                             " from CPAN -- sending N/A grade",
                             $prq_name, $name ), $verbose );

                    $grade = GRADE_NA;
                    last GRADE;
                }

                if ( !$obj ) {
                    my $vcore = $sub->( $prq_name );
                    if ( $cb->_vcmp( $prq_ver, $vcore ) > 0 ) {
                      msg(loc( "Version of core module '%1' ('%2') is too low for ".
                               "'%3' (needs '%4') -- sending N/A grade",
                               $prq_name, $vcore,
                               $name, $prq_ver ), $verbose );

                      $grade = GRADE_NA;
                      last GRADE;
                    }
                }

                if( $obj and $cb->_vcmp( $prq_ver, $obj->installed_version ) > 0 ) {
                    msg(loc( "Installed version of '%1' ('%2') is too low for ".
                             "'%3' (needs '%4') -- sending N/A grade",
                             $prq_name, $obj->installed_version,
                             $name, $prq_ver ), $verbose );

                    $grade = GRADE_NA;
                    last GRADE;
                }
            }
        }

        unless( RELEVANT_TEST_RESULT->($mod) ) {
            msg(loc(
                "'%1' is a platform specific module, and the test results on".
                " your platform are not relevant --sending N/A grade.",
                $name), $verbose);

            $grade = GRADE_NA;

        } elsif ( UNSUPPORTED_OS->( $buffer ) ) {
            msg(loc(
                "'%1' is a platform specific module, and the test results on".
                " your platform are not relevant --sending N/A grade.",
                $name), $verbose);

            $grade = GRADE_NA;

        ### you dont have a high enough perl version?
        } elsif ( PERL_VERSION_TOO_LOW->( $buffer ) ) {
            msg(loc("'%1' requires a higher version of perl than your current ".
                    "version -- sending N/A grade.", $name), $verbose);

            $grade = GRADE_NA;

        ### perhaps where were no tests...
        ### see if the thing even had tests ###
        } elsif ( NO_TESTS_DEFINED->( $buffer ) ) {
            $grade = GRADE_UNKNOWN;
        ### failures in PL or make/build stage are now considered UNKNOWN
        } elsif ( $stage !~ /\btest\b/ ) {

            $grade = GRADE_UNKNOWN

        } else {

            $grade = GRADE_FAIL;
        }

    ### if we got here, it didn't fail and tests were present.. so a PASS
    ### is in order
    } else {
        $grade = GRADE_PASS;
    } }

    ### so an error occurred, let's see what stage it went wrong in ###

    ### the header -- always include so the CPANPLUS version is apparent
    my $message =  REPORT_MESSAGE_HEADER->( $int_ver, $author );

    if( $grade eq GRADE_FAIL or $grade eq GRADE_UNKNOWN) {

        ### return if one or more missing external libraries
        if( my @missing = MISSING_EXTLIBS_LIST->($buffer) ) {
            msg(loc("Not sending test report - " .
                    "external libraries not pre-installed"));
            return 1;
        }

        ### return if we're only supposed to report make_test failures ###
        return 1 if $cp_conf =~  /\bmaketest_only\b/i
                    and ($stage !~ /\btest\b/);

        my $capture = ( $status && defined $status->{capture} ? $status->{capture} : $buffer );
        ### the bit where we inform what went wrong
        $message .= REPORT_MESSAGE_FAIL_HEADER->( $stage, $capture );

        ### was it missing prereqs? ###
        if( my @missing = MISSING_PREREQS_LIST->($buffer) ) {
            if(!$self->_verify_missing_prereqs(
                                module  => $mod,
                                missing => \@missing
                        )) {
                msg(loc("Not sending test report - "  .
                        "bogus missing prerequisites report"));
                return 1;
            }
            $message .= REPORT_MISSING_PREREQS->($author,$email,@missing);
        }

        ### was it missing test files? ###
        if( NO_TESTS_DEFINED->($buffer) ) {
            $message .= REPORT_MISSING_TESTS->();
        }

        ### add a list of what modules have been loaded of your prereqs list
        $message .= REPORT_LOADED_PREREQS->($mod);

        ### add a list of versions of toolchain modules
        $message .= REPORT_TOOLCHAIN_VERSIONS->($mod);

        ### the footer
        $message .= REPORT_MESSAGE_FOOTER->();

    ### it may be another grade than fail/unknown.. may be worth noting
    ### that tests got skipped, since the buffer is not added in
    } elsif ( $tests_skipped ) {
        $message .= REPORT_TESTS_SKIPPED->();
    } elsif( $grade eq GRADE_NA) {

        my $capture = ( $status && defined $status->{capture} ? $status->{capture} : $buffer );

        ### add the reason for the NA to the buffer
        $capture = join $/, $capture, map {
                        '[' . $_->tag . '] [' . $_->when . '] ' .
                        $_->message } ( CPANPLUS::Error->stack )[-1];

        ### the bit where we inform what went wrong
        $message .= REPORT_MESSAGE_FAIL_HEADER->( $stage, $capture );

        ### add a list of what modules have been loaded of your prereqs list
        $message .= REPORT_LOADED_PREREQS->($mod);

        ### add a list of versions of toolchain modules
        $message .= REPORT_TOOLCHAIN_VERSIONS->($mod);

        ### the footer
        $message .= REPORT_MESSAGE_FOOTER->();

    } elsif ( $grade eq GRADE_PASS and ( $status and defined $status->{capture} ) ) {
        ### the bit where we inform what went right
        $message .= REPORT_MESSAGE_PASS_HEADER->( $stage, $status->{capture} );

        ### add a list of what modules have been loaded of your prereqs list
        $message .= REPORT_LOADED_PREREQS->($mod);

        ### add a list of versions of toolchain modules
        $message .= REPORT_TOOLCHAIN_VERSIONS->($mod);

        ### the footer
        $message .= REPORT_MESSAGE_FOOTER->();

    }

    msg( loc("Sending test report for '%1'", $dist), $verbose);

    ### reporter object ###
    my $reporter = do {
        my $args = $conf->get_conf('cpantest_reporter_args') || {};

        unless( UNIVERSAL::isa( $args, 'HASH' ) ) {
            error(loc("'%1' must be a hashref, ignoring...",
                      'cpantest_reporter_args'));
            $args = {};
        }

        Test::Reporter->new(
            grade           => $grade,
            distribution    => $dist,
            distfile        => $distfile,
            via             => "CPANPLUS $int_ver",
            timeout         => $conf->get_conf('timeout') || 60,
            debug           => $conf->get_conf('debug'),
            %$args,
        );
    };

    ### set a custom mx, if requested
    $reporter->mx( [ $conf->get_conf('cpantest_mx') ] )
        if $conf->get_conf('cpantest_mx');

    ### set the from address ###
    $reporter->from( $conf->get_conf('email') )
        if $conf->get_conf('email') !~ /\@example\.\w+$/i;

    ### give the user a chance to programatically alter the message
    $message = $self->_callbacks->munge_test_report->($mod, $message, $grade);

    ### add the body if we have any ###
    $reporter->comments( $message ) if defined $message && length $message;

    ### do a callback to ask if we should send the report
    unless ($self->_callbacks->send_test_report->($mod, $grade)) {
        msg(loc("Ok, not sending test report"));
        return 1;
    }

    ### do a callback to ask if we should edit the report
    if ($self->_callbacks->edit_test_report->($mod, $grade)) {
        ### test::reporter 1.20 and lower don't have a way to set
        ### the preferred editor with a method call, but it does
        ### respect your env variable, so let's set that.
        local $ENV{VISUAL} = $conf->get_program('editor')
                                if $conf->get_program('editor');

        $reporter->edit_comments;
    }

    ### allow to be overridden, but default to the normal address ###
    $reporter->address( $address );

    ### should we save it locally? ###
    if( $save ) {
        if( my $file = $reporter->write() ) {
            msg(loc("Successfully wrote report for '%1' to '%2'",
                    $dist, $file), $verbose);
            return $file;

        } else {
            error(loc("Failed to write report for '%1'", $dist));
            return;
        }

    ### XXX should we do an 'already sent' check? ###
    ### something broke :( ###
    }
    else {
        my $status;
        eval {
            $status = $reporter->send();
        };
        if ( $@ ) {
           error(loc("Could not send '%1' report for '%2': %3",
                $grade, $dist, $@));
           return;
        }
        if ( $status ) {
           msg(loc("Successfully sent '%1' report for '%2'", $grade, $dist),
              $verbose);
           return 1;
        }
        error(loc("Could not send '%1' report for '%2': %3",
                $grade, $dist, $reporter->errstr));
        return;
    }
}

sub _verify_missing_prereqs {
    my $self = shift;
    my %hash = @_;

    ### check arguments ###
    my ($mod, $missing);
    my $tmpl = {
            module  => { required => 1, store => \$mod },
            missing => { required => 1, store => \$missing },
    };

    check( $tmpl, \%hash ) or return;


    my %missing = map {$_ => 1} @$missing;
    my $conf = $self->configure_object;
    my $extract = $mod->status->extract;

    ### Read pre-requisites from Makefile.PL or Build.PL (if there is one),
    ### of the form:
    ###     'PREREQ_PM' => {
    ###                      'Compress::Zlib'        => '1.20',
    ###                      'Test::More'            => 0,
    ###                    },
    ###  Build.PL uses 'requires' instead of 'PREREQ_PM'.

    my @search;
    push @search, ($extract ? MAKEFILE_PL->( $extract ) : MAKEFILE_PL->());
    push @search, ($extract ? BUILD_PL->( $extract ) : BUILD_PL->());

    for my $file ( @search ) {
        if(-e $file and -r $file) {
            my $slurp = $self->_get_file_contents(file => $file);
            my ($prereq) =
                ($slurp =~ /'?(?:PREREQ_PM|requires)'?\s*=>\s*{(.*?)}/s);
            my @prereq =
                ($prereq =~ /'?([\w\:]+)'?\s*=>\s*'?\d[\d\.\-\_]*'?/sg);
            delete $missing{$_} for(@prereq);
        }
    }

    return 1    if(keys %missing);  # There ARE missing prerequisites
    return;                         # All prerequisites accounted for
}

1;


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
