# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 4 -*-
package CPAN::FirstTime;
use strict;

use ExtUtils::MakeMaker ();
use FileHandle ();
use File::Basename ();
use File::Path ();
use File::Spec ();
use CPAN::Mirrors ();
use vars qw($VERSION $silent);
$VERSION = "5.5301";

=head1 NAME

CPAN::FirstTime - Utility for CPAN::Config file Initialization

=head1 SYNOPSIS

CPAN::FirstTime::init()

=head1 DESCRIPTION

The init routine asks a few questions and writes a CPAN/Config.pm or
CPAN/MyConfig.pm file (depending on what it is currently using).

In the following all questions and explanations regarding config
variables are collected.

=cut

# down until the next =back the manpage must be parsed by the program
# because the text is used in the init dialogues.

my @podpara = split /\n\n/, <<'=back';

=over 2

=item auto_commit

Normally CPAN.pm keeps config variables in memory and changes need to
be saved in a separate 'o conf commit' command to make them permanent
between sessions. If you set the 'auto_commit' option to true, changes
to a config variable are always automatically committed to disk.

Always commit changes to config variables to disk?

=item build_cache

CPAN.pm can limit the size of the disk area for keeping the build
directories with all the intermediate files.

Cache size for build directory (in MB)?

=item build_dir

Directory where the build process takes place?

=item build_dir_reuse

Until version 1.88 CPAN.pm never trusted the contents of the build_dir
directory between sessions. Since 1.88_58 CPAN.pm has a YAML-based
mechanism that makes it possible to share the contents of the
build_dir/ directory between different sessions with the same version
of perl. People who prefer to test things several days before
installing will like this feature because it safes a lot of time.

If you say yes to the following question, CPAN will try to store
enough information about the build process so that it can pick up in
future sessions at the same state of affairs as it left a previous
session.

Store and re-use state information about distributions between
CPAN.pm sessions?

=item build_requires_install_policy

When a module declares another one as a 'build_requires' prerequisite
this means that the other module is only needed for building or
testing the module but need not be installed permanently. In this case
you may wish to install that other module nonetheless or just keep it
in the 'build_dir' directory to have it available only temporarily.
Installing saves time on future installations but makes the perl
installation bigger.

You can choose if you want to always install (yes), never install (no)
or be always asked. In the latter case you can set the default answer
for the question to yes (ask/yes) or no (ask/no).

Policy on installing 'build_requires' modules (yes, no, ask/yes,
ask/no)?

=item cache_metadata

To considerably speed up the initial CPAN shell startup, it is
possible to use Storable to create a cache of metadata. If Storable is
not available, the normal index mechanism will be used.

Note: this mechanism is not used when use_sqlite is on and SQLLite is
running.

Cache metadata (yes/no)?

=item check_sigs

CPAN packages can be digitally signed by authors and thus verified
with the security provided by strong cryptography. The exact mechanism
is defined in the Module::Signature module. While this is generally
considered a good thing, it is not always convenient to the end user
to install modules that are signed incorrectly or where the key of the
author is not available or where some prerequisite for
Module::Signature has a bug and so on.

With the check_sigs parameter you can turn signature checking on and
off. The default is off for now because the whole tool chain for the
functionality is not yet considered mature by some. The author of
CPAN.pm would recommend setting it to true most of the time and
turning it off only if it turns out to be annoying.

Note that if you do not have Module::Signature installed, no signature
checks will be performed at all.

Always try to check and verify signatures if a SIGNATURE file is in
the package and Module::Signature is installed (yes/no)?

=item colorize_output

When you have Term::ANSIColor installed, you can turn on colorized
output to have some visual differences between normal CPAN.pm output,
warnings, debugging output, and the output of the modules being
installed. Set your favorite colors after some experimenting with the
Term::ANSIColor module.

Do you want to turn on colored output?

=item colorize_print

Color for normal output?

=item colorize_warn

Color for warnings?

=item colorize_debug

Color for debugging messages?

=item commandnumber_in_prompt

The prompt of the cpan shell can contain the current command number
for easier tracking of the session or be a plain string.

Do you want the command number in the prompt (yes/no)?

=item connect_to_internet_ok

If you have never defined your own C<urllist> in your configuration
then C<CPAN.pm> will be hesitant to use the built in default sites for
downloading. It will ask you once per session if a connection to the
internet is OK and only if you say yes, it will try to connect. But to
avoid this question, you can choose your favorite download sites once
and get away with it. Or, if you have no favorite download sites
answer yes to the following question.

If no urllist has been chosen yet, would you prefer CPAN.pm to connect
to the built-in default sites without asking? (yes/no)?

=item ftp_passive

Shall we always set the FTP_PASSIVE environment variable when dealing
with ftp download (yes/no)?

=item ftpstats_period

Statistics about downloads are truncated by size and period
simultaneously.

How many days shall we keep statistics about downloads?

=item ftpstats_size

Statistics about downloads are truncated by size and period
simultaneously.

How many items shall we keep in the statistics about downloads?

=item getcwd

CPAN.pm changes the current working directory often and needs to
determine its own current working directory. Per default it uses
Cwd::cwd but if this doesn't work on your system for some reason,
alternatives can be configured according to the following table:

    cwd         Cwd::cwd
    getcwd      Cwd::getcwd
    fastcwd     Cwd::fastcwd
    backtickcwd external command cwd

Preferred method for determining the current working directory?

=item halt_on_failure

Normaly, CPAN.pm continues processing the full list of targets and
dependencies, even if one of them fails.  However, you can specify 
that CPAN should halt after the first failure. 

Do you want to halt on failure (yes/no)?

=item histfile

If you have one of the readline packages (Term::ReadLine::Perl,
Term::ReadLine::Gnu, possibly others) installed, the interactive CPAN
shell will have history support. The next two questions deal with the
filename of the history file and with its size. If you do not want to
set this variable, please hit SPACE RETURN to the following question.

File to save your history?

=item histsize

Number of lines to save?

=item inactivity_timeout

Sometimes you may wish to leave the processes run by CPAN alone
without caring about them. Because the Makefile.PL or the Build.PL
sometimes contains question you're expected to answer, you can set a
timer that will kill a 'perl Makefile.PL' process after the specified
time in seconds.

If you set this value to 0, these processes will wait forever. This is
the default and recommended setting.

Timeout for inactivity during {Makefile,Build}.PL?

=item index_expire

The CPAN indexes are usually rebuilt once or twice per hour, but the
typical CPAN mirror mirrors only once or twice per day. Depending on
the quality of your mirror and your desire to be on the bleeding edge,
you may want to set the following value to more or less than one day
(which is the default). It determines after how many days CPAN.pm
downloads new indexes.

Let the index expire after how many days?

=item inhibit_startup_message

When the CPAN shell is started it normally displays a greeting message
that contains the running version and the status of readline support.

Do you want to turn this message off?

=item keep_source_where

Unless you are accessing the CPAN on your filesystem via a file: URL,
CPAN.pm needs to keep the source files it downloads somewhere. Please
supply a directory where the downloaded files are to be kept.

Download target directory?

=item load_module_verbosity

When CPAN.pm loads a module it needs for some optional feature, it
usually reports about module name and version. Choose 'v' to get this
message, 'none' to suppress it.

Verbosity level for loading modules (none or v)?

=item makepl_arg

Every Makefile.PL is run by perl in a separate process. Likewise we
run 'make' and 'make install' in separate processes. If you have
any parameters (e.g. PREFIX, UNINST or the like) you want to
pass to the calls, please specify them here.

If you don't understand this question, just press ENTER.

Typical frequently used settings:

    PREFIX=~/perl    # non-root users (please see manual for more hints)

Parameters for the 'perl Makefile.PL' command?

=item make_arg

Parameters for the 'make' command? Typical frequently used setting:

    -j3              # dual processor system (on GNU make)

Your choice:

=item make_install_arg

Parameters for the 'make install' command?
Typical frequently used setting:

    UNINST=1         # to always uninstall potentially conflicting files

Your choice:

=item make_install_make_command

Do you want to use a different make command for 'make install'?
Cautious people will probably prefer:

    su root -c make
 or
    sudo make
 or
    /path1/to/sudo -u admin_account /path2/to/make

or some such. Your choice:

=item mbuildpl_arg

A Build.PL is run by perl in a separate process. Likewise we run
'./Build' and './Build install' in separate processes. If you have any
parameters you want to pass to the calls, please specify them here.

Typical frequently used settings:

    --install_base /home/xxx             # different installation directory

Parameters for the 'perl Build.PL' command?

=item mbuild_arg

Parameters for the './Build' command? Setting might be:

    --extra_linker_flags -L/usr/foo/lib  # non-standard library location

Your choice:

=item mbuild_install_arg

Parameters for the './Build install' command? Typical frequently used
setting:

    --uninst 1                           # uninstall conflicting files

Your choice:

=item mbuild_install_build_command

Do you want to use a different command for './Build install'? Sudo
users will probably prefer:

    su root -c ./Build
 or
    sudo ./Build
 or
    /path1/to/sudo -u admin_account ./Build

or some such. Your choice:

=item pager

What is your favorite pager program?

=item prefer_installer

When you have Module::Build installed and a module comes with both a
Makefile.PL and a Build.PL, which shall have precedence?

The main two standard installer modules are the old and well
established ExtUtils::MakeMaker (for short: EUMM) which uses the
Makefile.PL. And the next generation installer Module::Build (MB)
which works with the Build.PL (and often comes with a Makefile.PL
too). If a module comes only with one of the two we will use that one
but if both are supplied then a decision must be made between EUMM and
MB. See also http://rt.cpan.org/Ticket/Display.html?id=29235 for a
discussion about the right default.

Or, as a third option you can choose RAND which will make a random
decision (something regular CPAN testers will enjoy).

In case you can choose between running a Makefile.PL or a Build.PL,
which installer would you prefer (EUMM or MB or RAND)?

=item prefs_dir

CPAN.pm can store customized build environments based on regular
expressions for distribution names. These are YAML files where the
default options for CPAN.pm and the environment can be overridden and
dialog sequences can be stored that can later be executed by an
Expect.pm object. The CPAN.pm distribution comes with some prefab YAML
files that cover sample distributions that can be used as blueprints
to store one own prefs. Please check out the distroprefs/ directory of
the CPAN.pm distribution to get a quick start into the prefs system.

Directory where to store default options/environment/dialogs for
building modules that need some customization?

=item prerequisites_policy

The CPAN module can detect when a module which you are trying to build
depends on prerequisites. If this happens, it can build the
prerequisites for you automatically ('follow'), ask you for
confirmation ('ask'), or just ignore them ('ignore').  Choosing
'follow' also sets PERL_AUTOINSTALL and PERL_EXTUTILS_AUTOINSTALL for
"--defaultdeps" if not already set.

Please set your policy to one of the three values.

Policy on building prerequisites (follow, ask or ignore)?

=item randomize_urllist

CPAN.pm can introduce some randomness when using hosts for download
that are configured in the urllist parameter. Enter a numeric value
between 0 and 1 to indicate how often you want to let CPAN.pm try a
random host from the urllist. A value of one specifies to always use a
random host as the first try. A value of zero means no randomness at
all. Anything in between specifies how often, on average, a random
host should be tried first.

Randomize parameter

=item scan_cache

By default, each time the CPAN module is started, cache scanning is
performed to keep the cache size in sync. To prevent this, answer
'never'.

Perform cache scanning (atstart or never)?

=item shell

What is your favorite shell?

=item show_unparsable_versions

During the 'r' command CPAN.pm finds modules without version number.
When the command finishes, it prints a report about this. If you
want this report to be very verbose, say yes to the following
variable.

Show all individual modules that have no $VERSION?

=item show_upload_date

The 'd' and the 'm' command normally only show you information they
have in their in-memory database and thus will never connect to the
internet. If you set the 'show_upload_date' variable to true, 'm' and
'd' will additionally show you the upload date of the module or
distribution. Per default this feature is off because it may require a
net connection to get at the upload date.

Always try to show upload date with 'd' and 'm' command (yes/no)?

=item show_zero_versions

During the 'r' command CPAN.pm finds modules with a version number of
zero. When the command finishes, it prints a report about this. If you
want this report to be very verbose, say yes to the following
variable.

Show all individual modules that have a $VERSION of zero?

=item tar_verbosity

When CPAN.pm uses the tar command, which switch for the verbosity
shall be used? Choose 'none' for quiet operation, 'v' for file
name listing, 'vv' for full listing.

Tar command verbosity level (none or v or vv)?

=item term_is_latin

The next option deals with the charset (aka character set) your
terminal supports. In general, CPAN is English speaking territory, so
the charset does not matter much but some CPAN have names that are
outside the ASCII range. If your terminal supports UTF-8, you should
say no to the next question. If it expects ISO-8859-1 (also known as
LATIN1) then you should say yes. If it supports neither, your answer
does not matter because you will not be able to read the names of some
authors anyway. If you answer no, names will be output in UTF-8.

Your terminal expects ISO-8859-1 (yes/no)?

=item term_ornaments

When using Term::ReadLine, you can turn ornaments on so that your
input stands out against the output from CPAN.pm.

Do you want to turn ornaments on?

=item test_report

The goal of the CPAN Testers project (http://testers.cpan.org/) is to
test as many CPAN packages as possible on as many platforms as
possible.  This provides valuable feedback to module authors and
potential users to identify bugs or platform compatibility issues and
improves the overall quality and value of CPAN.

One way you can contribute is to send test results for each module
that you install.  If you install the CPAN::Reporter module, you have
the option to automatically generate and email test reports to CPAN
Testers whenever you run tests on a CPAN package.

See the CPAN::Reporter documentation for additional details and
configuration settings.  If your firewall blocks outgoing email,
you will need to configure CPAN::Reporter before sending reports.

Email test reports if CPAN::Reporter is installed (yes/no)?

=item perl5lib_verbosity

When CPAN.pm extends @INC via PERL5LIB, it prints a list of
directories added (or a summary of how many directories are
added).  Choose 'v' to get this message, 'none' to suppress it.

Verbosity level for PERL5LIB changes (none or v)?

=item trust_test_report_history

When a distribution has already been tested by CPAN::Reporter on
this machine, CPAN can skip the test phase and just rely on the
test report history instead.

Note that this will not apply to distributions that failed tests
because of missing dependencies.  Also, tests can be run
regardless of the history using "force".

Do you want to rely on the test report history (yes/no)?

=item use_sqlite

CPAN::SQLite is a layer between the index files that are downloaded
from the CPAN and CPAN.pm that speeds up metadata queries and reduces
memory consumption of CPAN.pm considerably.

Use CPAN::SQLite if available? (yes/no)?

=item version_timeout

This timeout prevents CPAN from hanging when trying to parse a
pathologically coded $VERSION from a module.

The default is 15 seconds.  If you set this value to 0, no timeout
will occur, but this is not recommended.

Timeout for parsing module versions?

=item yaml_load_code

Both YAML.pm and YAML::Syck are capable of deserialising code. As this
requires a string eval, which might be a security risk, you can use
this option to enable or disable the deserialisation of code via
CPAN::DeferredCode. (Note: This does not work under perl 5.6)

Do you want to enable code deserialisation (yes/no)?

=item yaml_module

At the time of this writing (2009-03) there are three YAML
implementations working: YAML, YAML::Syck, and YAML::XS. The latter
two are faster but need a C compiler installed on your system. There
may be more alternative YAML conforming modules. When I tried two
other players, YAML::Tiny and YAML::Perl, they seemed not powerful
enough to work with CPAN.pm. This may have changed in the meantime.

Which YAML implementation would you prefer?

=back

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

use vars qw( %prompts );

{

    my @prompts = (

manual_config => qq[
CPAN is the world-wide archive of perl resources. It consists of about
300 sites that all replicate the same contents around the globe. Many
countries have at least one CPAN site already. The resources found on
CPAN are easily accessible with the CPAN.pm module. If you want to use
CPAN.pm, lots of things have to be configured. Fortunately, most of
them can be determined automatically. If you prefer the automatic
configuration, answer 'yes' below.

If you prefer to enter a dialog instead, you can answer 'no' to this
question and I'll let you configure in small steps one thing after the
other. (Note: you can revisit this dialog anytime later by typing 'o
conf init' at the cpan prompt.)

],

auto_pick => qq{
Would you like me to automatically choose the best CPAN mirror
sites for you? (This means connecting to the Internet and could
take a couple minutes)},

config_intro => qq{

The following questions are intended to help you with the
configuration. The CPAN module needs a directory of its own to cache
important index files and maybe keep a temporary mirror of CPAN files.
This may be a site-wide or a personal directory.

},

# cpan_home => qq{ },

cpan_home_where => qq{

First of all, I'd like to create this directory. Where?

},

external_progs => qq{

The CPAN module will need a few external programs to work properly.
Please correct me, if I guess the wrong path for a program. Don't
panic if you do not have some of them, just press ENTER for those. To
disable the use of a program, you can type a space followed by ENTER.

},

proxy_intro => qq{

If you're accessing the net via proxies, you can specify them in the
CPAN configuration or via environment variables. The variable in
the \$CPAN::Config takes precedence.

},

proxy_user => qq{

If your proxy is an authenticating proxy, you can store your username
permanently. If you do not want that, just press RETURN. You will then
be asked for your username in every future session.

},

proxy_pass => qq{

Your password for the authenticating proxy can also be stored
permanently on disk. If this violates your security policy, just press
RETURN. You will then be asked for the password in every future
session.

},

urls_intro => qq{
Now you need to choose your CPAN mirror sites.  You can let me
pick mirrors for you, you can select them from a list or you
can enter them by hand.
},

urls_picker_intro => qq{First, pick a nearby continent and country by typing in the number(s)
in front of the item(s) you want to select. You can pick several of
each, separated by spaces. Then, you will be presented with a list of
URLs of CPAN mirrors in the countries you selected, along with
previously selected URLs. Select some of those URLs, or just keep the
old list. Finally, you will be prompted for any extra URLs -- file:,
ftp:, or http: -- that host a CPAN mirror.

You should select more than one (just in case the first isn't available).

},

password_warn => qq{

Warning: Term::ReadKey seems not to be available, your password will
be echoed to the terminal!

},

              );

    die "Coding error in \@prompts declaration.  Odd number of elements, above"
        if (@prompts % 2);

    %prompts = @prompts;

    if (scalar(keys %prompts) != scalar(@prompts)/2) {
        my %already;
        for my $item (0..$#prompts) {
            next if $item % 2;
            die "$prompts[$item] is duplicated\n" if $already{$prompts[$item]}++;
        }
    }

    shift @podpara;
    while (@podpara) {
        warn "Alert: cannot parse my own manpage for init dialog" unless $podpara[0] =~ s/^=item\s+//;
        my $name = shift @podpara;
        my @para;
        while (@podpara && $podpara[0] !~ /^=item/) {
            push @para, shift @podpara;
        }
        $prompts{$name} = pop @para;
        if (@para) {
            $prompts{$name . "_intro"} = join "", map { "$_\n\n" } @para;
        }
    }

}

sub init {
    my($configpm, %args) = @_;
    use Config;
    # extra args after 'o conf init'
    my $matcher = $args{args} && @{$args{args}} ? $args{args}[0] : '';
    if ($matcher =~ /^\/(.*)\/$/) {
        # case /regex/ => take the first, ignore the rest
        $matcher = $1;
        shift @{$args{args}};
        if (@{$args{args}}) {
            local $" = " ";
            $CPAN::Frontend->mywarn("Ignoring excessive arguments '@{$args{args}}'");
            $CPAN::Frontend->mysleep(2);
        }
    } elsif (0 == length $matcher) {
    } elsif (0 && $matcher eq "~") { # extremely buggy, but a nice idea
        my @unconfigured = grep { not exists $CPAN::Config->{$_}
                                      or not defined $CPAN::Config->{$_}
                                          or not length $CPAN::Config->{$_}
                                  } keys %$CPAN::Config;
        $matcher = "\\b(".join("|", @unconfigured).")\\b";
        $CPAN::Frontend->mywarn("matcher[$matcher]");
    } else {
        # case WORD... => all arguments must be valid
        for my $arg (@{$args{args}}) {
            unless (exists $CPAN::HandleConfig::keys{$arg}) {
                $CPAN::Frontend->mywarn("'$arg' is not a valid configuration variable\n");
                return;
            }
        }
        $matcher = "\\b(".join("|",@{$args{args}}).")\\b";
    }
    CPAN->debug("matcher[$matcher]") if $CPAN::DEBUG;

    unless ($CPAN::VERSION) {
        require CPAN::Nox;
    }
    require CPAN::HandleConfig;
    CPAN::HandleConfig::require_myconfig_or_config();
    $CPAN::Config ||= {};
    local($/) = "\n";
    local($\) = "";
    local($|) = 1;

    my($ans,$default); # why so half global?

    #
    #= Files, directories
    #

    unless ($matcher) {
        $CPAN::Frontend->myprint($prompts{manual_config});
    }

    my $manual_conf;

    local *_real_prompt;
    if ( $args{autoconfig} ) {
        $manual_conf = "no";
    } elsif ($matcher) {
        $manual_conf = "yes";
    } else {
        my $_conf = prompt("Would you like me to configure as much as possible ".
                           "automatically?", "yes");
        $manual_conf = ($_conf and $_conf =~ /^y/i) ? "no" : "yes";
    }
    CPAN->debug("manual_conf[$manual_conf]") if $CPAN::DEBUG;
    my $fastread;
    {
        if ($manual_conf =~ /^y/i) {
            $fastread = 0;
        } else {
            $fastread = 1;
            $silent = 1;
            $CPAN::Config->{urllist} ||= [];
            $CPAN::Config->{connect_to_internet_ok} ||= 1;

            local $^W = 0;
            # prototype should match that of &MakeMaker::prompt
            my $current_second = time;
            my $current_second_count = 0;
            my $i_am_mad = 0;
            # silent prompting -- just quietly use default
            *_real_prompt = sub { return $_[1] };
        }
    }

    if (!$matcher or q{
                       build_dir
                       build_dir_reuse
                       cpan_home
                       keep_source_where
                       prefs_dir
                      } =~ /$matcher/) {
        $CPAN::Frontend->myprint($prompts{config_intro}) unless $silent;

        init_cpan_home($matcher);

        my_dflt_prompt("keep_source_where",
                       File::Spec->catdir($CPAN::Config->{cpan_home},"sources"),
                       $matcher,
                      );
        my_dflt_prompt("build_dir",
                       File::Spec->catdir($CPAN::Config->{cpan_home},"build"),
                       $matcher
                      );
        my_yn_prompt(build_dir_reuse => 0, $matcher);
        my_dflt_prompt("prefs_dir",
                       File::Spec->catdir($CPAN::Config->{cpan_home},"prefs"),
                       $matcher
                      );
    }

    #
    #= Config: auto_commit
    #

    my_yn_prompt(auto_commit => 0, $matcher);

    #
    #= Cache size, Index expire
    #
    my_dflt_prompt(build_cache => 100, $matcher);

    my_dflt_prompt(index_expire => 1, $matcher);
    my_prompt_loop(scan_cache => 'atstart', $matcher, 'atstart|never');

    #
    #= cache_metadata
    #

    my_yn_prompt(cache_metadata => 1, $matcher);
    my_yn_prompt(use_sqlite => 0, $matcher);

    #
    #= Do we follow PREREQ_PM?
    #

    my_prompt_loop(prerequisites_policy => 'follow', $matcher,
                   'follow|ask|ignore');
    my_prompt_loop(build_requires_install_policy => 'yes', $matcher,
                   'yes|no|ask/yes|ask/no');

    #
    #= Module::Signature
    #
    my_yn_prompt(check_sigs => 0, $matcher);

    #
    #= CPAN::Reporter
    #
    if (!$matcher or 'test_report' =~ /$matcher/) {
        my_yn_prompt(test_report => 0, $matcher);
        if (
            $CPAN::Config->{test_report} &&
            $CPAN::META->has_inst("CPAN::Reporter") &&
            CPAN::Reporter->can('configure')
           ) {
            local *_real_prompt;
            *_real_prompt = \&CPAN::Shell::colorable_makemaker_prompt;
            my $_conf = prompt("Would you like me configure CPAN::Reporter now?", $silent ? "no" : "yes");
            if ($_conf =~ /^y/i) {
              $CPAN::Frontend->myprint("\nProceeding to configure CPAN::Reporter.\n");
              CPAN::Reporter::configure();
              $CPAN::Frontend->myprint("\nReturning to CPAN configuration.\n");
            }
        }
    }

    my_yn_prompt(trust_test_report_history => 0, $matcher);

    #
    #= YAML vs. YAML::Syck
    #
    if (!$matcher or "yaml_module" =~ /$matcher/) {
        my_dflt_prompt(yaml_module => "YAML", $matcher);
        my $old_v = $CPAN::Config->{load_module_verbosity};
        $CPAN::Config->{load_module_verbosity} = q[none];
        if (!$silent && !$CPAN::META->has_inst($CPAN::Config->{yaml_module})) {
            $CPAN::Frontend->mywarn
                ("Warning (maybe harmless): '$CPAN::Config->{yaml_module}' not installed.\n");
            $CPAN::Frontend->mysleep(3);
        }
        $CPAN::Config->{load_module_verbosity} = $old_v;
    }

    #
    #= YAML code deserialisation
    #
    my_yn_prompt(yaml_load_code => 0, $matcher);

    #
    #= External programs
    #
    my(@path) = split /$Config{'path_sep'}/, $ENV{'PATH'};
    _init_external_progs($matcher,\@path);

    {
        my $path = $CPAN::Config->{'pager'} ||
            $ENV{PAGER} || find_exe("less",\@path) ||
                find_exe("more",\@path) || ($^O eq 'MacOS' ? $ENV{EDITOR} : 0 )
                    || "more";
        my_dflt_prompt(pager => $path, $matcher);
    }

    {
        my $path = $CPAN::Config->{'shell'};
        if ($path && File::Spec->file_name_is_absolute($path)) {
            $CPAN::Frontend->mywarn("Warning: configured $path does not exist\n")
                unless -e $path;
            $path = "";
        }
        $path ||= $ENV{SHELL};
        $path ||= $ENV{COMSPEC} if $^O eq "MSWin32";
        if ($^O eq 'MacOS') {
            $CPAN::Config->{'shell'} = 'not_here';
        } else {
            $path ||= 'sh', $path =~ s,\\,/,g if $^O eq 'os2'; # Cosmetic only
            my_dflt_prompt(shell => $path, $matcher);
        }
    }

    #
    # verbosity
    #

    my_prompt_loop(tar_verbosity => 'none', $matcher,
                   'none|v|vv');
    my_prompt_loop(load_module_verbosity => 'none', $matcher,
                   'none|v');
    my_prompt_loop(perl5lib_verbosity => 'none', $matcher,
                   'none|v');
    my_yn_prompt(inhibit_startup_message => 0, $matcher);

    #
    #= Installer, arguments to make etc.
    #

    my_prompt_loop(prefer_installer => 'MB', $matcher, 'MB|EUMM|RAND');

    if (!$matcher or 'makepl_arg make_arg' =~ /$matcher/) {
        my_dflt_prompt(makepl_arg => "", $matcher);
        my_dflt_prompt(make_arg => "", $matcher);
        if ( $CPAN::Config->{makepl_arg} =~ /LIBS=|INC=/ ) {
            $CPAN::Frontend->mywarn( 
                "Warning: Using LIBS or INC in makepl_arg will likely break distributions\n" . 
                "that specify their own LIBS or INC options in Makefile.PL.\n"
            );
        }

    }

    require CPAN::HandleConfig;
    if (exists $CPAN::HandleConfig::keys{make_install_make_command}) {
        # as long as Windows needs $self->_build_command, we cannot
        # support sudo on windows :-)
        my_dflt_prompt(make_install_make_command => $CPAN::Config->{make} || "",
                       $matcher);
    }

    my_dflt_prompt(make_install_arg => $CPAN::Config->{make_arg} || "",
                   $matcher);

    my_dflt_prompt(mbuildpl_arg => "", $matcher);
    my_dflt_prompt(mbuild_arg => "", $matcher);

    if (exists $CPAN::HandleConfig::keys{mbuild_install_build_command}
        and $^O ne "MSWin32") {
        # as long as Windows needs $self->_build_command, we cannot
        # support sudo on windows :-)
        my_dflt_prompt(mbuild_install_build_command => "./Build", $matcher);
    }

    my_dflt_prompt(mbuild_install_arg => "", $matcher);

    #
    #= Alarm period
    #

    my_dflt_prompt(inactivity_timeout => 0, $matcher);
    my_dflt_prompt(version_timeout => 15, $matcher);

    #
    #== halt_on_failure
    #
    my_yn_prompt(halt_on_failure => 0, $matcher);

    #
    #= Proxies
    #

    my @proxy_vars = qw/ftp_proxy http_proxy no_proxy/;
    my @proxy_user_vars = qw/proxy_user proxy_pass/;
    if (!$matcher or "@proxy_vars @proxy_user_vars" =~ /$matcher/) {
        $CPAN::Frontend->myprint($prompts{proxy_intro}) unless $silent;

        for (@proxy_vars) {
            $prompts{$_} = "Your $_?";
            my_dflt_prompt($_ => $ENV{$_}||"", $matcher);
        }

        if ($CPAN::Config->{ftp_proxy} ||
            $CPAN::Config->{http_proxy}) {

            $default = $CPAN::Config->{proxy_user} || $CPAN::LWP::UserAgent::USER || "";

            $CPAN::Frontend->myprint($prompts{proxy_user}) unless $silent;

            if ($CPAN::Config->{proxy_user} = prompt("Your proxy user id?",$default)) {
                $CPAN::Frontend->myprint($prompts{proxy_pass}) unless $silent;

                if ($CPAN::META->has_inst("Term::ReadKey")) {
                    Term::ReadKey::ReadMode("noecho");
                } else {
                    $CPAN::Frontend->myprint($prompts{password_warn}) unless $silent;
                }
                $CPAN::Config->{proxy_pass} = prompt_no_strip("Your proxy password?");
                if ($CPAN::META->has_inst("Term::ReadKey")) {
                    Term::ReadKey::ReadMode("restore");
                }
                $CPAN::Frontend->myprint("\n\n") unless $silent;
            }
        }
    }

    #
    #= how FTP works
    #

    my_yn_prompt(ftp_passive => 1, $matcher);

    #
    #= how cwd works
    #

    my_prompt_loop(getcwd => 'cwd', $matcher,
                   'cwd|getcwd|fastcwd|backtickcwd');

    #
    #= the CPAN shell itself (prompt, color)
    #

    my_yn_prompt(commandnumber_in_prompt => 1, $matcher);
    my_yn_prompt(term_ornaments => 1, $matcher);
    if ("colorize_output colorize_print colorize_warn colorize_debug" =~ $matcher) {
        my_yn_prompt(colorize_output => 0, $matcher);
        if ($CPAN::Config->{colorize_output}) {
            if ($CPAN::META->has_inst("Term::ANSIColor")) {
                my $T="gYw";
                $CPAN::Frontend->myprint( "                                      on_  on_y ".
                    "        on_ma           on_\n") unless $silent;
                $CPAN::Frontend->myprint( "                   on_black on_red  green ellow ".
                    "on_blue genta on_cyan white\n") unless $silent;

                for my $FG ("", "bold",
                            map {$_,"bold $_"} "black","red","green",
                            "yellow","blue",
                            "magenta",
                            "cyan","white") {
                    $CPAN::Frontend->myprint(sprintf( "%12s ", $FG)) unless $silent;
                    for my $BG ("",map {"on_$_"} qw(black red green yellow
                                                    blue magenta cyan white)) {
                            $CPAN::Frontend->myprint( $FG||$BG ?
                            Term::ANSIColor::colored("  $T  ","$FG $BG") : "  $T  ") unless $silent;
                    }
                    $CPAN::Frontend->myprint( "\n" ) unless $silent;
                }
                $CPAN::Frontend->myprint( "\n" ) unless $silent;
            }
            for my $tuple (
                           ["colorize_print", "bold blue on_white"],
                           ["colorize_warn", "bold red on_white"],
                           ["colorize_debug", "black on_cyan"],
                          ) {
                my_dflt_prompt($tuple->[0] => $tuple->[1], $matcher);
                if ($CPAN::META->has_inst("Term::ANSIColor")) {
                    eval { Term::ANSIColor::color($CPAN::Config->{$tuple->[0]})};
                    if ($@) {
                        $CPAN::Config->{$tuple->[0]} = $tuple->[1];
                        $CPAN::Frontend->mywarn($@."setting to default '$tuple->[1]'\n");
                    }
                }
            }
        }
    }

    #
    #== term_is_latin
    #

    my_yn_prompt(term_is_latin => 1, $matcher);

    #
    #== save history in file 'histfile'
    #

    if (!$matcher or 'histfile histsize' =~ /$matcher/) {
        $CPAN::Frontend->myprint($prompts{histfile_intro}) unless $silent;
        defined($default = $CPAN::Config->{histfile}) or
            $default = File::Spec->catfile($CPAN::Config->{cpan_home},"histfile");
        my_dflt_prompt(histfile => $default, $matcher);

        if ($CPAN::Config->{histfile}) {
            defined($default = $CPAN::Config->{histsize}) or $default = 100;
            my_dflt_prompt(histsize => $default, $matcher);
        }
    }

    #
    #== do an ls on the m or the d command
    #
    my_yn_prompt(show_upload_date => 0, $matcher);

    #
    #== verbosity at the end of the r command
    #
    if (!$matcher
        or 'show_unparsable_versions' =~ /$matcher/
        or 'show_zero_versions' =~ /$matcher/
       ) {
        $CPAN::Frontend->myprint($prompts{show_unparsable_or_zero_versions_intro});
        my_yn_prompt(show_unparsable_versions => 0, $matcher);
        my_yn_prompt(show_zero_versions => 0, $matcher);
    }

    #
    #= MIRRORED.BY and conf_sites()
    #

    # remember, this is only triggered if no urllist is given, so 0 is
    # fair and protects the default site from being overloaded and
    # gives the user more chances to select his own urllist.
    my_yn_prompt("connect_to_internet_ok" => $fastread ? 1 : 0, $matcher);
    $CPAN::Config->{urllist} ||= [];
    if ($matcher) {
        if ("urllist" =~ $matcher) {
            $CPAN::Frontend->myprint($prompts{urls_intro});

            # conf_sites would go into endless loop with the smash prompt
            local *_real_prompt;
            *_real_prompt = \&CPAN::Shell::colorable_makemaker_prompt;
            my $_conf = prompt($prompts{auto_pick}, "yes");

            if ( $_conf =~ /^y/i ) {
              conf_sites( auto_pick => 1 ) or bring_your_own();
            }
            else {
              my $_conf = prompt(
                "Would you like to pick from the CPAN mirror list?", "yes"
              );

              if ( $_conf =~ /^y/i ) {
                conf_sites();
              }
              bring_your_own();
            }
            _print_urllist();
        }
        if ("randomize_urllist" =~ $matcher) {
            my_dflt_prompt(randomize_urllist => 0, $matcher);
        }
        if ("ftpstats_size" =~ $matcher) {
            my_dflt_prompt(ftpstats_size => 99, $matcher);
        }
        if ("ftpstats_period" =~ $matcher) {
            my_dflt_prompt(ftpstats_period => 14, $matcher);
        }
    } elsif ($fastread) {
        $silent = 0;
        local *_real_prompt;
        *_real_prompt = \&CPAN::Shell::colorable_makemaker_prompt;
        if ( @{ $CPAN::Config->{urllist} } ) {
            $CPAN::Frontend->myprint(
              "\nYour 'urllist' is already configured. Type 'o conf init urllist' to change it.\n"
            );
        }
        else {
          $CPAN::Frontend->myprint(
            "Autoconfigured everything but 'urllist'.\n"
          );

          $CPAN::Frontend->myprint($prompts{urls_intro});

          my $_conf = prompt($prompts{auto_pick}, "yes");

          if ( $_conf =~ /^y/i ) {
            conf_sites( auto_pick => 1 ) or bring_your_own();
          }
          else {
            my $_conf = prompt(
              "Would you like to pick from the CPAN mirror list?", "yes"
            );

            if ( $_conf =~ /^y/i ) {
              conf_sites();
            }
            bring_your_own();
          }
          _print_urllist();
        }
        $CPAN::Frontend->myprint(
            "\nAutoconfiguration complete.\n"
        );
    }

    $silent = 0; # reset

    $CPAN::Frontend->myprint("\n");
    if ($matcher && !$CPAN::Config->{auto_commit}) {
        $CPAN::Frontend->myprint("Please remember to call 'o conf commit' to ".
                                 "make the config permanent!\n");
    } else {
        CPAN::HandleConfig->commit($configpm);
    }
}

sub _init_external_progs {
    my($matcher,$PATH) = @_;
    my @external_progs = qw/bzip2 gzip tar unzip

                            make

                            curl lynx wget ncftpget ncftp ftp

                            gpg

                            patch applypatch
                            /;
    if (!$matcher or "@external_progs" =~ /$matcher/) {
        $CPAN::Frontend->myprint($prompts{external_progs}) unless $silent;

        my $old_warn = $^W;
        local $^W if $^O eq 'MacOS';
        local $^W = $old_warn;
        my $progname;
        for $progname (@external_progs) {
            next if $matcher && $progname !~ /$matcher/;
            if ($^O eq 'MacOS') {
                $CPAN::Config->{$progname} = 'not_here';
                next;
            }

            my $progcall = $progname;
            unless ($matcher) {
                # we really don't need ncftp if we have ncftpget, but
                # if they chose this dialog via matcher, they shall have it
                next if $progname eq "ncftp" && $CPAN::Config->{ncftpget} gt " ";
            }
            my $path = $CPAN::Config->{$progname}
                || $Config::Config{$progname}
                    || "";
            if (File::Spec->file_name_is_absolute($path)) {
                # testing existence is not good enough, some have these exe
                # extensions

                # warn "Warning: configured $path does not exist\n" unless -e $path;
                # $path = "";
            } elsif ($path =~ /^\s+$/) {
                # preserve disabled programs
            } else {
                $path = '';
            }
            unless ($path) {
                # e.g. make -> nmake
                $progcall = $Config::Config{$progname} if $Config::Config{$progname};
            }

            $path ||= find_exe($progcall,$PATH);
            unless ($path) { # not -e $path, because find_exe already checked that
                local $"=";";
                $CPAN::Frontend->mywarn("Warning: $progcall not found in PATH[@$PATH]\n") unless $silent;
                if ($progname eq "make") {
                    $CPAN::Frontend->mywarn("ALERT: 'make' is an essential tool for ".
                                            "building perl Modules. Please make sure you ".
                                            "have 'make' (or some equivalent) ".
                                            "working.\n"
                                           );
                    if ($^O eq "MSWin32") {
                        $CPAN::Frontend->mywarn("
Windows users may want to follow this procedure when back in the CPAN shell:

    look YVES/scripts/alien_nmake.pl
    perl alien_nmake.pl

This will install nmake on your system which can be used as a 'make'
substitute. You can then revisit this dialog with

    o conf init make

");
                    }
                }
            }
            $prompts{$progname} = "Where is your $progname program?";
            my_dflt_prompt($progname,$path,$matcher);
        }
    }
}

sub init_cpan_home {
    my($matcher) = @_;
    if (!$matcher or 'cpan_home' =~ /$matcher/) {
        my $cpan_home = $CPAN::Config->{cpan_home}
            || File::Spec->catdir(CPAN::HandleConfig::home(), ".cpan");

        if (-d $cpan_home) {
            $CPAN::Frontend->myprint(qq{

I see you already have a  directory
    $cpan_home
Shall we use it as the general CPAN build and cache directory?

}) unless $silent;
        } else {
            # no cpan-home, must prompt and get one
            $CPAN::Frontend->myprint($prompts{cpan_home_where}) unless $silent;
        }

        my $default = $cpan_home;
        my $loop = 0;
        my($last_ans,$ans);
        $CPAN::Frontend->myprint(" <cpan_home>\n") unless $silent;
    PROMPT: while ($ans = prompt("CPAN build and cache directory?",$default)) {
            if (File::Spec->file_name_is_absolute($ans)) {
                my @cpan_home = split /[\/\\]/, $ans;
            DIR: for my $dir (@cpan_home) {
                    if ($dir =~ /^~/ and (!$last_ans or $ans ne $last_ans)) {
                        $CPAN::Frontend
                            ->mywarn("Warning: a tilde in the path will be ".
                                     "taken as a literal tilde. Please ".
                                     "confirm again if you want to keep it\n");
                        $last_ans = $default = $ans;
                        next PROMPT;
                    }
                }
            } else {
                require Cwd;
                my $cwd = Cwd::cwd();
                my $absans = File::Spec->catdir($cwd,$ans);
                $CPAN::Frontend->mywarn("The path '$ans' is not an ".
                                        "absolute path. Please specify ".
                                        "an absolute path\n");
                $default = $absans;
                next PROMPT;
            }
            eval { File::Path::mkpath($ans); }; # dies if it can't
            if ($@) {
                $CPAN::Frontend->mywarn("Couldn't create directory $ans.\n".
                                        "Please retry.\n");
                next PROMPT;
            }
            if (-d $ans && -w _) {
                last PROMPT;
            } else {
                $CPAN::Frontend->mywarn("Couldn't find directory $ans\n".
                                        "or directory is not writable. Please retry.\n");
                if (++$loop > 5) {
                    $CPAN::Frontend->mydie("Giving up");
                }
            }
        }
        $CPAN::Config->{cpan_home} = $ans;
    }
}

sub my_dflt_prompt {
    my ($item, $dflt, $m) = @_;
    my $default = $CPAN::Config->{$item} || $dflt;

    if (!$silent && (!$m || $item =~ /$m/)) {
        if (my $intro = $prompts{$item . "_intro"}) {
            $CPAN::Frontend->myprint($intro);
        }
        $CPAN::Frontend->myprint(" <$item>\n");
        $CPAN::Config->{$item} = prompt($prompts{$item}, $default);
    } else {
        $CPAN::Config->{$item} = $default;
    }
}

sub my_yn_prompt {
    my ($item, $dflt, $m) = @_;
    my $default;
    defined($default = $CPAN::Config->{$item}) or $default = $dflt;

    # $DB::single = 1;
    if (!$silent && (!$m || $item =~ /$m/)) {
        if (my $intro = $prompts{$item . "_intro"}) {
            $CPAN::Frontend->myprint($intro);
        }
        $CPAN::Frontend->myprint(" <$item>\n");
        my $ans = prompt($prompts{$item}, $default ? 'yes' : 'no');
        $CPAN::Config->{$item} = ($ans =~ /^[y1]/i ? 1 : 0);
    } else {
        $CPAN::Config->{$item} = $default;
    }
}

sub my_prompt_loop {
    my ($item, $dflt, $m, $ok) = @_;
    my $default = $CPAN::Config->{$item} || $dflt;
    my $ans;

    if (!$silent && (!$m || $item =~ /$m/)) {
        $CPAN::Frontend->myprint($prompts{$item . "_intro"});
        $CPAN::Frontend->myprint(" <$item>\n");
        do { $ans = prompt($prompts{$item}, $default);
        } until $ans =~ /$ok/;
        $CPAN::Config->{$item} = $ans;
    } else {
        $CPAN::Config->{$item} = $default;
    }
}


# Here's the logic about the MIRRORED.BY file.  There are a number of scenarios:
# (1) We have a cached MIRRORED.BY file
#   (1a) We're auto-picking
#       - Refresh it automatically if it's old
#   (1b) Otherwise, ask if using cached is ok.  If old, default to no.
#       - If cached is not ok, get it from the Internet. If it succeeds we use
#         the new file.  Otherwise, we use the old file.
# (2) We don't have a copy at all
#   (2a) If we are allowed to connect, we try to get a new copy.  If it succeeds,
#        we use it, otherwise, we warn about failure
#   (2b) If we aren't allowed to connect, 

sub conf_sites {
    my %args = @_;
    # auto pick implies using the internet
    $CPAN::Config->{connect_to_internet_ok} = 1 if $args{auto_pick};

    my $m = 'MIRRORED.BY';
    my $mby = File::Spec->catfile($CPAN::Config->{keep_source_where},$m);
    File::Path::mkpath(File::Basename::dirname($mby));
    # Why are we using MIRRORED.BY from the current directory?
    # Is this for testing? -- dagolden, 2009-11-05
    if (-f $mby && -f $m && -M $m < -M $mby) {
        require File::Copy;
        File::Copy::copy($m,$mby) or die "Could not update $mby: $!";
    }
    local $^T = time;
    # if we have a cached copy is not older than 60 days, we either
    # use it or refresh it or fall back to it if the refresh failed.
    if ($mby && -f $mby && -s _ > 0 ) {
      my $very_old = (-M $mby > 60);
      my $mtime = localtime((stat _)[9]);
      # if auto_pick, refresh anything old automatically
      if ( $args{auto_pick} ) {
        if ( $very_old ) {
          $CPAN::Frontend->myprint(qq{Trying to refresh your mirror list\n});
          eval { CPAN::FTP->localize($m,$mby,3,1) }
            or $CPAN::Frontend->myprint(qq{Refresh failed.  Using the old cached copy instead.\n});
          $CPAN::Frontend->myprint("\n");
        }
      }
      else {
        my $prompt = qq{Found a cached mirror list as of $mtime

If you'd like to just use the cached copy, answer 'yes', below.
If you'd like an updated copy of the mirror list, answer 'no' and
I'll get a fresh one from the Internet.

Shall I use the cached mirror list?};
        my $ans = prompt($prompt, $very_old ? "no" : "yes");
        if ($ans =~ /^n/i) {
          $CPAN::Frontend->myprint(qq{Trying to refresh your mirror list\n});
          # you asked for it from the Internet
          $CPAN::Config->{connect_to_internet_ok} = 1;
          eval { CPAN::FTP->localize($m,$mby,3,1) }
            or $CPAN::Frontend->myprint(qq{Refresh failed.  Using the old cached copy instead.\n});
          $CPAN::Frontend->myprint("\n");
        }
      }
    }
    # else there is no cached copy and we must fetch or fail
    else {
      # If they haven't agree to connect to the internet, ask again
      if ( ! $CPAN::Config->{connect_to_internet_ok} ) {
        my $prompt = q{You are missing a copy of the CPAN mirror list.

May I connect to the Internet to get it?};
        my $ans = prompt($prompt, "yes");
        if ($ans =~ /^y/i) {
          $CPAN::Config->{connect_to_internet_ok} = 1;
        }
      }

      # Now get it from the Internet or complain
      if ( $CPAN::Config->{connect_to_internet_ok} ) {
        $CPAN::Frontend->myprint(qq{Trying to fetch a mirror list from the Internet\n});
        eval { CPAN::FTP->localize($m,$mby,3,1) }
          or $CPAN::Frontend->mywarn(<<'HERE');
We failed to get a copy of the mirror list from the Internet.
You will need to provide CPAN mirror URLs yourself.
HERE
        $CPAN::Frontend->myprint("\n");
      }
      else {
        $CPAN::Frontend->mywarn(<<'HERE');
You will need to provide CPAN mirror URLs yourself or set 
'o conf connect_to_internet_ok 1' and try again.
HERE
      }
    }

    # if we finally have a good local MIRRORED.BY, get on with picking
    if (-f $mby && -s _ > 0){
        $CPAN::Config->{urllist} =
          $args{auto_pick} ? auto_mirrored_by($mby) : choose_mirrored_by($mby);
        return 1;
    }

    return;
}

sub find_exe {
    my($exe,$path) = @_;
    my($dir);
    #warn "in find_exe exe[$exe] path[@$path]";
    for $dir (@$path) {
        my $abs = File::Spec->catfile($dir,$exe);
        if (($abs = MM->maybe_command($abs))) {
            return $abs;
        }
    }
}

sub picklist {
    my($items,$prompt,$default,$require_nonempty,$empty_warning)=@_;
    CPAN->debug("picklist('$items','$prompt','$default','$require_nonempty',".
                "'$empty_warning')") if $CPAN::DEBUG;
    $default ||= '';

    my $pos = 0;

    my @nums;
  SELECTION: while (1) {

        # display, at most, 15 items at a time
        my $limit = $#{ $items } - $pos;
        $limit = 15 if $limit > 15;

        # show the next $limit items, get the new position
        $pos = display_some($items, $limit, $pos, $default);
        $pos = 0 if $pos >= @$items;

        my $num = prompt($prompt,$default);

        @nums = split (' ', $num);
        {
            my %seen;
            @nums = grep { !$seen{$_}++ } @nums;
        }
        my $i = scalar @$items;
        unrangify(\@nums);
        if (0 == @nums) {
            # cannot allow nothing because nothing means paging!
            # return;
        } elsif (grep (/\D/ || $_ < 1 || $_ > $i, @nums)) {
            $CPAN::Frontend->mywarn("invalid items entered, try again\n");
            if ("@nums" =~ /\D/) {
                $CPAN::Frontend->mywarn("(we are expecting only numbers between 1 and $i)\n");
            }
            next SELECTION;
        }
        if ($require_nonempty && !@nums) {
            $CPAN::Frontend->mywarn("$empty_warning\n");
        }

        # a blank line continues...
        unless (@nums){
            $CPAN::Frontend->mysleep(0.1); # prevent hot spinning process on the next bug
            next SELECTION;
        }
        last;
    }
    for (@nums) { $_-- }
    @{$items}[@nums];
}

sub unrangify ($) {
    my($nums) = $_[0];
    my @nums2 = ();
    while (@{$nums||[]}) {
        my $n = shift @$nums;
        if ($n =~ /^(\d+)-(\d+)$/) {
            my @range = $1 .. $2;
            # warn "range[@range]";
            push @nums2, @range;
        } else {
            push @nums2, $n;
        }
    }
    push @$nums, @nums2;
}

sub display_some {
    my ($items, $limit, $pos, $default) = @_;
    $pos ||= 0;

    my @displayable = @$items[$pos .. ($pos + $limit)];
    for my $item (@displayable) {
        $CPAN::Frontend->myprint(sprintf "(%d) %s\n", ++$pos, $item);
    }
    my $hit_what = $default ? "SPACE RETURN" : "RETURN";
    $CPAN::Frontend->myprint(sprintf("%d more items, hit %s to show them\n",
                                     (@$items - $pos),
                                     $hit_what,
                                    ))
        if $pos < @$items;
    return $pos;
}

sub auto_mirrored_by {
    my $local = shift or return;
    local $|=1;
    $CPAN::Frontend->myprint("Searching for the best CPAN mirrors (please be patient) ...");
    my $mirrors = CPAN::Mirrors->new($local);
    my $cnt = 0;
    my @best = $mirrors->best_mirrors(
      how_many => 5,
      callback => sub { $CPAN::Frontend->myprint(".") },
    );
    my $urllist = [ map { $_->ftp } @best ];
    push @$urllist, grep { /^file:/ } @{$CPAN::Config->{urllist}};
    $CPAN::Frontend->myprint(" done!\n\n");
    return $urllist;
}

sub choose_mirrored_by {
    my $local = shift or return;
    my ($default);
    my $mirrors = CPAN::Mirrors->new($local);
    my @previous_urls = @{$CPAN::Config->{urllist}};

    $CPAN::Frontend->myprint($prompts{urls_picker_intro});

    my (@cont, $cont, %cont, @countries, @urls, %seen);
    my $no_previous_warn =
        "Sorry! since you don't have any existing picks, you must make a\n" .
            "geographic selection.";
    my $offer_cont = [sort $mirrors->continents];
    if (@previous_urls) {
        push @$offer_cont, "(edit previous picks)";
        $default = @$offer_cont;
    } else {
        # cannot allow nothing because nothing means paging!
        # push @$offer_cont, "(none of the above)";
    }
    @cont = picklist($offer_cont,
                     "Select your continent (or several nearby continents)",
                     $default,
                     ! @previous_urls,
                     $no_previous_warn);
    # cannot allow nothing because nothing means paging!
    # return unless @cont;

    foreach $cont (@cont) {
        my @c = sort $mirrors->countries($cont);
        @cont{@c} = map ($cont, 0..$#c);
        @c = map ("$_ ($cont)", @c) if @cont > 1;
        push (@countries, @c);
    }
    if (@previous_urls && @countries) {
        push @countries, "(edit previous picks)";
        $default = @countries;
    }

    if (@countries) {
        @countries = picklist (\@countries,
                               "Select your country (or several nearby countries)",
                               $default,
                               ! @previous_urls,
                               $no_previous_warn);
        %seen = map (($_ => 1), @previous_urls);
        # hmmm, should take list of defaults from CPAN::Config->{'urllist'}...
        foreach my $country (@countries) {
            next if $country =~ /edit previous picks/;
            (my $bare_country = $country) =~ s/ \(.*\)//;
            my @u;
            for my $m ( $mirrors->mirrors($bare_country) ) {
              push @u, $m->ftp if $m->ftp;
              push @u, $m->http if $m->http;
            }
            @u = grep (! $seen{$_}, @u);
            @u = map ("$_ ($bare_country)", @u)
                if @countries > 1;
            push (@urls, sort @u);
        }
    }
    push (@urls, map ("$_ (previous pick)", @previous_urls));
    my $prompt = "Select as many URLs as you like (by number),
put them on one line, separated by blanks, hyphenated ranges allowed
 e.g. '1 4 5' or '7 1-4 8'";
    if (@previous_urls) {
        $default = join (' ', ((scalar @urls) - (scalar @previous_urls) + 1) ..
                         (scalar @urls));
        $prompt .= "\n(or just hit RETURN to keep your previous picks)";
    }

    @urls = picklist (\@urls, $prompt, $default);
    foreach (@urls) { s/ \(.*\)//; }
    return [ @urls ];
}

sub bring_your_own {
    my $urllist = [ @{$CPAN::Config->{urllist}} ];
    my %seen = map (($_ => 1), @$urllist);
    my($ans,@urls);
    my $eacnt = 0; # empty answers
    $CPAN::Frontend->myprint(<<'HERE');

Now you can enter your own CPAN URLs by hand. A local CPAN mirror can be
listed using a 'file:' URL like 'file:///path/to/cpan/'

HERE
    do {
        my $prompt = "Enter another URL or RETURN to quit:";
        unless (%seen) {
            $prompt = qq{CPAN.pm needs at least one URL where it can fetch CPAN files from.

Please enter your CPAN site:};
        }
        $ans = prompt ($prompt, "");

        if ($ans) {
            $ans =~ s|/?\z|/|; # has to end with one slash
            # XXX This manipulation is odd.  Shouldn't we check that $ans is
            # a directory before converting to file:///?  And we need /// below,
            # too, don't we?  -- dagolden, 2009-11-05
            $ans = "file:$ans" unless $ans =~ /:/; # without a scheme is a file:
            if ($ans =~ /^\w+:\/./) {
                push @urls, $ans unless $seen{$ans}++;
            } else {
                $CPAN::Frontend->
                    myprint(sprintf(qq{"%s" doesn\'t look like an URL at first sight.
I\'ll ignore it for now.
You can add it to your %s
later if you\'re sure it\'s right.\n},
                                   $ans,
                                   $INC{'CPAN/MyConfig.pm'}
                                   || $INC{'CPAN/Config.pm'}
                                   || "configuration file",
                                  ));
            }
        } else {
            if (++$eacnt >= 5) {
                $CPAN::Frontend->
                    mywarn("Giving up.\n");
                $CPAN::Frontend->mysleep(5);
                return;
            }
        }
    } while $ans || !%seen;

    @$urllist = CPAN::_uniq(@$urllist, @urls);
    $CPAN::Config->{urllist} = $urllist;
}

sub _print_urllist {
    $CPAN::Frontend->myprint("New urllist\n");
    for ( @{$CPAN::Config->{urllist} || []} ) { 
      $CPAN::Frontend->myprint("  $_\n") 
    };
}

sub _strip_spaces {
    $_[0] =~ s/^\s+//;  # no leading spaces
    $_[0] =~ s/\s+\z//; # no trailing spaces
}

sub prompt ($;$) {
    unless (defined &_real_prompt) {
        *_real_prompt = \&CPAN::Shell::colorable_makemaker_prompt;
    }
    my $ans = _real_prompt(@_);

    _strip_spaces($ans);
    $CPAN::Frontend->myprint("\n");

    return $ans;
}


sub prompt_no_strip ($;$) {
    return _real_prompt(@_);
}



1;
