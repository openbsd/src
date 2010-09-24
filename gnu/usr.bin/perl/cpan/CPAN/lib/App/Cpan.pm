package App::Cpan;
use strict;
use warnings;
use vars qw($VERSION);

$VERSION = '1.5701';

=head1 NAME

App::Cpan - easily interact with CPAN from the command line

=head1 SYNOPSIS

	# with arguments and no switches, installs specified modules
	cpan module_name [ module_name ... ]

	# with switches, installs modules with extra behavior
	cpan [-cfFimt] module_name [ module_name ... ]

	# use local::lib
	cpan -l module_name [ module_name ... ]
	
	# with just the dot, install from the distribution in the
	# current directory
	cpan .
	
	# without arguments, starts CPAN.pm shell
	cpan

	# without arguments, but some switches
	cpan [-ahruvACDLO]

=head1 DESCRIPTION

This script provides a command interface (not a shell) to CPAN. At the
moment it uses CPAN.pm to do the work, but it is not a one-shot command
runner for CPAN.pm.

=head2 Options

=over 4

=item -a

Creates a CPAN.pm autobundle with CPAN::Shell->autobundle.

=item -A module [ module ... ]

Shows the primary maintainers for the specified modules.

=item -c module

Runs a `make clean` in the specified module's directories.

=item -C module [ module ... ]

Show the F<Changes> files for the specified modules

=item -D module [ module ... ]

Show the module details. This prints one line for each out-of-date module
(meaning, modules locally installed but have newer versions on CPAN).
Each line has three columns: module name, local version, and CPAN
version.

=item -f

Force the specified action, when it normally would have failed. Use this
to install a module even if its tests fail. When you use this option,
-i is not optional for installing a module when you need to force it:

	% cpan -f -i Module::Foo

=item -F

Turn off CPAN.pm's attempts to lock anything. You should be careful with 
this since you might end up with multiple scripts trying to muck in the
same directory. This isn't so much of a concern if you're loading a special
config with C<-j>, and that config sets up its own work directories.

=item -g module [ module ... ]

Downloads to the current directory the latest distribution of the module.

=item -G module [ module ... ]

UNIMPLEMENTED

Download to the current directory the latest distribution of the
modules, unpack each distribution, and create a git repository for each
distribution.

If you want this feature, check out Yanick Champoux's C<Git::CPAN::Patch>
distribution.

=item -h

Print a help message and exit. When you specify C<-h>, it ignores all
of the other options and arguments.

=item -i

Install the specified modules.

=item -j Config.pm

Load the file that has the CPAN configuration data. This should have the
same format as the standard F<CPAN/Config.pm> file, which defines 
C<$CPAN::Config> as an anonymous hash.

=item -J

Dump the configuration in the same format that CPAN.pm uses. This is useful
for checking the configuration as well as using the dump as a starting point
for a new, custom configuration.

=item -l

Use C<local::lib>.

=item -L author [ author ... ]

List the modules by the specified authors.

=item -m

Make the specified modules.

=item -O

Show the out-of-date modules.

=item -t

Run a `make test` on the specified modules.

=item -r

Recompiles dynamically loaded modules with CPAN::Shell->recompile.

=item -u

Upgrade all installed modules. Blindly doing this can really break things,
so keep a backup.

=item -v

Print the script version and CPAN.pm version then exit.

=back

=head2 Examples

	# print a help message
	cpan -h

	# print the version numbers
	cpan -v

	# create an autobundle
	cpan -a

	# recompile modules
	cpan -r

	# upgrade all installed modules
	cpan -u

	# install modules ( sole -i is optional )
	cpan -i Netscape::Booksmarks Business::ISBN

	# force install modules ( must use -i )
	cpan -fi CGI::Minimal URI


=head2 Methods

=over 4

=cut

use autouse Carp => qw(carp croak cluck);
use CPAN ();
use autouse Cwd => qw(cwd);
use autouse 'Data::Dumper' => qw(Dumper);
use File::Spec::Functions;
use File::Basename;

use Getopt::Std;

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# Internal constants
use constant TRUE  => 1;
use constant FALSE => 0;


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# The return values
use constant HEY_IT_WORKED              =>   0; 
use constant I_DONT_KNOW_WHAT_HAPPENED  =>   1; # 0b0000_0001
use constant ITS_NOT_MY_FAULT           =>   2;
use constant THE_PROGRAMMERS_AN_IDIOT   =>   4;
use constant A_MODULE_FAILED_TO_INSTALL =>   8;


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# set up the order of options that we layer over CPAN::Shell
BEGIN { # most of this should be in methods
use vars qw( @META_OPTIONS $Default %CPAN_METHODS @CPAN_OPTIONS  @option_order
	%Method_table %Method_table_index );
	
@META_OPTIONS = qw( h v g G C A D O l L a r j: J );

$Default = 'default';

%CPAN_METHODS = ( # map switches to method names in CPAN::Shell
	$Default => 'install',
	'c'      => 'clean',
	'f'      => 'force',
	'i'      => 'install',
	'm'      => 'make',
	't'      => 'test',
	'u'      => 'upgrade',
	);
@CPAN_OPTIONS = grep { $_ ne $Default } sort keys %CPAN_METHODS;

@option_order = ( @META_OPTIONS, @CPAN_OPTIONS );


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# map switches to the subroutines in this script, along with other information.
# use this stuff instead of hard-coded indices and values
sub NO_ARGS   () { 0 }
sub ARGS      () { 1 }
sub GOOD_EXIT () { 0 }

%Method_table = (
# key => [ sub ref, takes args?, exit value, description ]

	# options that do their thing first, then exit
	h =>  [ \&_print_help,        NO_ARGS, GOOD_EXIT, 'Printing help'                ],
	v =>  [ \&_print_version,     NO_ARGS, GOOD_EXIT, 'Printing version'             ],

	# options that affect other options
	j =>  [ \&_load_config,          ARGS, GOOD_EXIT, 'Use specified config file'    ],
	J =>  [ \&_dump_config,       NO_ARGS, GOOD_EXIT, 'Dump configuration to stdout' ],
	F =>  [ \&_lock_lobotomy,     NO_ARGS, GOOD_EXIT, 'Turn off CPAN.pm lock files'  ],

	# options that do their one thing
	g =>  [ \&_download,          NO_ARGS, GOOD_EXIT, 'Download the latest distro'        ],
	G =>  [ \&_gitify,            NO_ARGS, GOOD_EXIT, 'Down and gitify the latest distro' ],
	
	C =>  [ \&_show_Changes,         ARGS, GOOD_EXIT, 'Showing Changes file'         ],
	A =>  [ \&_show_Author,          ARGS, GOOD_EXIT, 'Showing Author'               ],
	D =>  [ \&_show_Details,         ARGS, GOOD_EXIT, 'Showing Details'              ],
	O =>  [ \&_show_out_of_date,  NO_ARGS, GOOD_EXIT, 'Showing Out of date'          ],

	l =>  [ \&_list_all_mods,     NO_ARGS, GOOD_EXIT, 'Listing all modules'          ],

	L =>  [ \&_show_author_mods,     ARGS, GOOD_EXIT, 'Showing author mods'          ],
	a =>  [ \&_create_autobundle, NO_ARGS, GOOD_EXIT, 'Creating autobundle'          ],
	r =>  [ \&_recompile,         NO_ARGS, GOOD_EXIT, 'Recompiling'                  ],
	u =>  [ \&_upgrade,           NO_ARGS, GOOD_EXIT, 'Running `make test`'          ],

	c =>  [ \&_default,              ARGS, GOOD_EXIT, 'Running `make clean`'         ],
	f =>  [ \&_default,              ARGS, GOOD_EXIT, 'Installing with force'        ],
	i =>  [ \&_default,              ARGS, GOOD_EXIT, 'Running `make install`'       ],
   'm' => [ \&_default,              ARGS, GOOD_EXIT, 'Running `make`'               ],
	t =>  [ \&_default,              ARGS, GOOD_EXIT, 'Running `make test`'          ],

	);

%Method_table_index = (
	code        => 0,
	takes_args  => 1,
	exit_value  => 2,
	description => 3,
	);
}

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# finally, do some argument processing

sub _stupid_interface_hack_for_non_rtfmers
	{
	no warnings 'uninitialized';
	shift @ARGV if( $ARGV[0] eq 'install' and @ARGV > 1 )
	}
	
sub _process_options
	{
	my %options;
	
	# if no arguments, just drop into the shell
	if( 0 == @ARGV ) { CPAN::shell(); exit 0 }
	else
		{
		Getopt::Std::getopts(
		  join( '', @option_order ), \%options );    
		 \%options;
		}
	}

sub _process_setup_options
	{
	my( $class, $options ) = @_;
	
	if( $options->{j} )
		{
		$Method_table{j}[ $Method_table_index{code} ]->( $options->{j} );
		delete $options->{j};
		}
	else
		{
		# this is what CPAN.pm would do otherwise
		CPAN::HandleConfig->load(
			# be_silent  => 1, # candidate to be ripped out forever
			write_file => 0,
			);
		}
		
	if( $options->{F} )
		{
		$Method_table{F}[ $Method_table_index{code} ]->( $options->{F} );
		delete $options->{F};
		}

	my $option_count = grep { $options->{$_} } @option_order;
	no warnings 'uninitialized';
	$option_count -= $options->{'f'}; # don't count force
	
	# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
	# if there are no options, set -i (this line fixes RT ticket 16915)
	$options->{i}++ unless $option_count;
	}


=item run()

Just do it.

The C<run> method returns 0 on success and a postive number on 
failure. See the section on EXIT CODES for details on the values.

=cut

my $logger;

sub run
	{
	my $class = shift;

	my $return_value = HEY_IT_WORKED; # assume that things will work

	$logger = $class->_init_logger;
	$logger->debug( "Using logger from @{[ref $logger]}" );

	$class->_hook_into_CPANpm_report;
	$logger->debug( "Hooked into output" );

	$class->_stupid_interface_hack_for_non_rtfmers;
	$logger->debug( "Patched cargo culting" );

	my $options = $class->_process_options;
	$logger->debug( "Options are @{[Dumper($options)]}" );

	$class->_process_setup_options( $options );

	OPTION: foreach my $option ( @option_order )
		{	
		next unless $options->{$option};

		my( $sub, $takes_args, $description ) = 
			map { $Method_table{$option}[ $Method_table_index{$_} ] }
			qw( code takes_args );

		unless( ref $sub eq ref sub {} )
			{
			$return_value = THE_PROGRAMMERS_AN_IDIOT;
			last OPTION;
			}

		$logger->info( "$description -- ignoring other arguments" )
			if( @ARGV && ! $takes_args );
		
		$return_value = $sub->( \ @ARGV, $options );

		last;
		}

	return $return_value;
	}

{
package Local::Null::Logger;

sub new { bless \ my $x, $_[0] }
sub AUTOLOAD { shift; print "NullLogger: ", @_, $/ if $ENV{CPAN_NULL_LOGGER} }
sub DESTROY { 1 }
}

sub _init_logger
	{
	my $log4perl_loaded = eval "require Log::Log4perl; 1";
	
    unless( $log4perl_loaded )
        {
        $logger = Local::Null::Logger->new;
        return $logger;
        }
	
	my $LEVEL = $ENV{CPANSCRIPT_LOGLEVEL} || 'INFO';
	
	Log::Log4perl::init( \ <<"HERE" );
log4perl.rootLogger=$LEVEL, A1
log4perl.appender.A1=Log::Log4perl::Appender::Screen
log4perl.appender.A1.layout=PatternLayout
log4perl.appender.A1.layout.ConversionPattern=%m%n
HERE
	
	$logger = Log::Log4perl->get_logger( 'App::Cpan' );
	}
	
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
 # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

sub _default
	{
	my( $args, $options ) = @_;
	
	my $switch = '';

	# choose the option that we're going to use
	# we'll deal with 'f' (force) later, so skip it
	foreach my $option ( @CPAN_OPTIONS )
		{
		next if $option eq 'f';
		next unless $options->{$option};
		$switch = $option;
		last;
		}

	# 1. with no switches, but arguments, use the default switch (install)
	# 2. with no switches and no args, start the shell
	# 3. With a switch but no args, die! These switches need arguments.
	   if( not $switch and     @$args ) { $switch = $Default;  }
	elsif( not $switch and not @$args ) { return CPAN::shell() }
	elsif(     $switch and not @$args )
		{ die "Nothing to $CPAN_METHODS{$switch}!\n"; }

	# Get and check the method from CPAN::Shell
	my $method = $CPAN_METHODS{$switch};
	die "CPAN.pm cannot $method!\n" unless CPAN::Shell->can( $method );

	# call the CPAN::Shell method, with force if specified
	my $action = do {
		if( $options->{f} ) { sub { CPAN::Shell->force( $method, @_ ) } }
		else                { sub { CPAN::Shell->$method( @_ )        } }
		};
	
	# How do I handle exit codes for multiple arguments?
	my $errors = 0;
	
	foreach my $arg ( @$args ) 
		{		
		_clear_cpanpm_output();
		$action->( $arg );

		$errors += defined _cpanpm_output_indicates_failure();
		}

	$errors ? I_DONT_KNOW_WHAT_HAPPENED : HEY_IT_WORKED;
	}

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

=for comment

CPAN.pm sends all the good stuff either to STDOUT. I have to intercept
that output so I can find out what happened.

=cut

{
my $scalar = '';

sub _hook_into_CPANpm_report
	{
	no warnings 'redefine';
	
	*CPAN::Shell::myprint = sub {
		my($self,$what) = @_;
		$scalar .= $what if defined $what;
		$self->print_ornamented($what,
			$CPAN::Config->{colorize_print}||'bold blue on_white',
			);
		};

	*CPAN::Shell::mywarn = sub {
		my($self,$what) = @_;
		$scalar .= $what if defined $what;
		$self->print_ornamented($what, 
			$CPAN::Config->{colorize_warn}||'bold red on_white'
			);
		};

	}
	
sub _clear_cpanpm_output { $scalar = '' }
	
sub _get_cpanpm_output   { $scalar }

BEGIN {
my @skip_lines = (
	qr/^\QWarning \(usually harmless\)/,
	qr/\bwill not store persistent state\b/,
	qr(//hint//),
	qr/^\s+reports\s+/,
	);

sub _get_cpanpm_last_line
	{
	open my($fh), "<", \ $scalar;
	
	my @lines = <$fh>;
	
    # This is a bit ugly. Once we examine a line, we have to
    # examine the line before it and go through all of the same
    # regexes. I could do something fancy, but this works.
    REGEXES: {
	foreach my $regex ( @skip_lines )
		{
		if( $lines[-1] =~ m/$regex/ )
            {
            pop @lines;
            redo REGEXES; # we have to go through all of them for every line!
            }
		}
    }
    
    $logger->debug( "Last interesting line of CPAN.pm output is:\n\t$lines[-1]" );
    
	$lines[-1];
	}
}

BEGIN {
my $epic_fail_words = join '|',
	qw( Error stop(?:ping)? problems force not unsupported fail(?:ed)? );
	
sub _cpanpm_output_indicates_failure
	{
	my $last_line = _get_cpanpm_last_line();
	
	my $result = $last_line =~ /\b(?:$epic_fail_words)\b/i;
	$result || ();
	}
}
	
sub _cpanpm_output_indicates_success
	{
	my $last_line = _get_cpanpm_last_line();
	
	my $result = $last_line =~ /\b(?:\s+-- OK|PASS)\b/;
	$result || ();
	}
	
sub _cpanpm_output_is_vague
	{
	return FALSE if 
		_cpanpm_output_indicates_failure() || 
		_cpanpm_output_indicates_success();

	return TRUE;
	}

}

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
sub _print_help
	{
	$logger->info( "Use perldoc to read the documentation" );
	exec "perldoc $0";
	}
	
sub _print_version
	{
	$logger->info( 
		"$0 script version $VERSION, CPAN.pm version " . CPAN->VERSION );

	return HEY_IT_WORKED;
	}
	
sub _create_autobundle
	{
	$logger->info( 
		"Creating autobundle in $CPAN::Config->{cpan_home}/Bundle" );

	CPAN::Shell->autobundle;

	return HEY_IT_WORKED;
	}

sub _recompile
	{
	$logger->info( "Recompiling dynamically-loaded extensions" );

	CPAN::Shell->recompile;

	return HEY_IT_WORKED;
	}

sub _upgrade
	{
	$logger->info( "Upgrading all modules" );

	CPAN::Shell->upgrade();

	return HEY_IT_WORKED;
	}

sub _load_config # -j
	{	
	my $file = shift || '';
	
	# should I clear out any existing config here?
	$CPAN::Config = {};
	delete $INC{'CPAN/Config.pm'};
	croak( "Config file [$file] does not exist!\n" ) unless -e $file;
	
	my $rc = eval "require '$file'";

	# CPAN::HandleConfig::require_myconfig_or_config looks for this
	$INC{'CPAN/MyConfig.pm'} = 'fake out!';
	
	# CPAN::HandleConfig::load looks for this
	$CPAN::Config_loaded = 'fake out';
	
	croak( "Could not load [$file]: $@\n") unless $rc;
	
	return HEY_IT_WORKED;
	}

sub _dump_config
	{
	my $args = shift;
	require Data::Dumper;
	
	my $fh = $args->[0] || \*STDOUT;
		
	my $dd = Data::Dumper->new( 
		[$CPAN::Config], 
		['$CPAN::Config'] 
		);
		
	print $fh $dd->Dump, "\n1;\n__END__\n";
	
	return HEY_IT_WORKED;
	}

sub _lock_lobotomy
	{
	no warnings 'redefine';
	
	*CPAN::_flock    = sub { 1 };
	*CPAN::checklock = sub { 1 };

	return HEY_IT_WORKED;
	}
	
sub _download
	{	
	my $args = shift;
	
	local $CPAN::DEBUG = 1;
	
	my %paths;
	
	foreach my $module ( @$args )
		{
		$logger->info( "Checking $module" );
		my $path = CPAN::Shell->expand( "Module", $module )->cpan_file;
		
		$logger->debug( "Inst file would be $path\n" );
		
		$paths{$module} = _get_file( _make_path( $path ) );
		}
		
	return \%paths;
	}

sub _make_path { join "/", qw(authors id), $_[0] }
	
sub _get_file
	{
	my $path = shift;
	
	my $loaded = eval "require LWP::Simple; 1;";
	croak "You need LWP::Simple to use features that fetch files from CPAN\n"
		unless $loaded;
	
	my $file = substr $path, rindex( $path, '/' ) + 1;
	my $store_path = catfile( cwd(), $file );
	$logger->debug( "Store path is $store_path" );

	foreach my $site ( @{ $CPAN::Config->{urllist} } )
		{
		my $fetch_path = join "/", $site, $path;
		$logger->debug( "Trying $fetch_path" );
	    last if LWP::Simple::getstore( $fetch_path, $store_path );
		}

	return $store_path;
	}

sub _gitify
	{
	my $args = shift;
	
	my $loaded = eval "require Archive::Extract; 1;";
	croak "You need Archive::Extract to use features that gitify distributions\n"
		unless $loaded;
	
	my $starting_dir = cwd();
	
	foreach my $module ( @$args )
		{
		$logger->info( "Checking $module" );
		my $path = CPAN::Shell->expand( "Module", $module )->cpan_file;

		my $store_paths = _download( [ $module ] );
		$logger->debug( "gitify Store path is $store_paths->{$module}" );
		my $dirname = dirname( $store_paths->{$module} );	
	
		my $ae = Archive::Extract->new( archive => $store_paths->{$module} );
		$ae->extract( to => $dirname );
		
		chdir $ae->extract_path;
		
		my $git = $ENV{GIT_COMMAND} || '/usr/local/bin/git';
		croak "Could not find $git"    unless -e $git;
		croak "$git is not executable" unless -x $git;
		
		# can we do this in Pure Perl?
		system( $git, 'init'    );
		system( $git, qw( add . ) );
		system( $git, qw( commit -a -m ), 'initial import' );
		}
	
	chdir $starting_dir;

	return HEY_IT_WORKED;
	}

sub _show_Changes
	{
	my $args = shift;
	
	foreach my $arg ( @$args )
		{
		$logger->info( "Checking $arg\n" );
		
		my $module = eval { CPAN::Shell->expand( "Module", $arg ) };
		my $out = _get_cpanpm_output();
		
		next unless eval { $module->inst_file };
		#next if $module->uptodate;
	
		( my $id = $module->id() ) =~ s/::/\-/;
	
		my $url = "http://search.cpan.org/~" . lc( $module->userid ) . "/" .
			$id . "-" . $module->cpan_version() . "/";
	
		#print "URL: $url\n";
		_get_changes_file($url);
		}

	return HEY_IT_WORKED;
	}	
	
sub _get_changes_file
	{
	croak "Reading Changes files requires LWP::Simple and URI\n"
		unless eval "require LWP::Simple; require URI; 1";
	
    my $url = shift;

    my $content = LWP::Simple::get( $url );
    $logger->info( "Got $url ..." ) if defined $content;
	#print $content;
	
	my( $change_link ) = $content =~ m|<a href="(.*?)">Changes</a>|gi;
	
	my $changes_url = URI->new_abs( $change_link, $url );
 	$logger->debug( "Change link is: $changes_url" );

	my $changes =  LWP::Simple::get( $changes_url );

	print $changes;

	return HEY_IT_WORKED;
	}
	
sub _show_Author
	{	
	my $args = shift;
	
	foreach my $arg ( @$args )
		{
		my $module = CPAN::Shell->expand( "Module", $arg );
		unless( $module )
			{
			$logger->info( "Didn't find a $arg module, so no author!" );
			next;
			}
			
		my $author = CPAN::Shell->expand( "Author", $module->userid );
	
		next unless $module->userid;
	
		printf "%-25s %-8s %-25s %s\n", 
			$arg, $module->userid, $author->email, $author->fullname;
		}

	return HEY_IT_WORKED;
	}	

sub _show_Details
	{
	my $args = shift;
	
	foreach my $arg ( @$args )
		{
		my $module = CPAN::Shell->expand( "Module", $arg );
		my $author = CPAN::Shell->expand( "Author", $module->userid );
	
		next unless $module->userid;
	
		print "$arg\n", "-" x 73, "\n\t";
		print join "\n\t",
			$module->description ? $module->description : "(no description)",
			$module->cpan_file,
			$module->inst_file,
			'Installed: ' . $module->inst_version,
			'CPAN:      ' . $module->cpan_version . '  ' .
				($module->uptodate ? "" : "Not ") . "up to date",
			$author->fullname . " (" . $module->userid . ")",
			$author->email;
		print "\n\n";
		
		}
		
	return HEY_IT_WORKED;
	}	

sub _show_out_of_date
	{
	my @modules = CPAN::Shell->expand( "Module", "/./" );
		
	printf "%-40s  %6s  %6s\n", "Module Name", "Local", "CPAN";
	print "-" x 73, "\n";
	
	foreach my $module ( @modules )
		{
		next unless $module->inst_file;
		next if $module->uptodate;
		printf "%-40s  %.4f  %.4f\n",
			$module->id, 
			$module->inst_version ? $module->inst_version : '', 
			$module->cpan_version;
		}

	return HEY_IT_WORKED;
	}

sub _show_author_mods
	{
	my $args = shift;

	my %hash = map { lc $_, 1 } @$args;
	
	my @modules = CPAN::Shell->expand( "Module", "/./" );
	
	foreach my $module ( @modules )
		{
		next unless exists $hash{ lc $module->userid };
		print $module->id, "\n";
		}
	
	return HEY_IT_WORKED;
	}
	
sub _list_all_mods
	{
	require File::Find;
	
	my $args = shift;
	
	
	my $fh = \*STDOUT;
	
	INC: foreach my $inc ( @INC )
		{		
		my( $wanted, $reporter ) = _generator();
		File::Find::find( { wanted => $wanted }, $inc );
		
		my $count = 0;
		FILE: foreach my $file ( @{ $reporter->() } )
			{
			my $version = _parse_version_safely( $file );
			
			my $module_name = _path_to_module( $inc, $file );
			next FILE unless defined $module_name;
			
			print $fh "$module_name\t$version\n";
			
			#last if $count++ > 5;
			}
		}

	return HEY_IT_WORKED;
	}
	
sub _generator
	{			
	my @files = ();
	
	sub { push @files, 
		File::Spec->canonpath( $File::Find::name ) 
		if m/\A\w+\.pm\z/ },
	sub { \@files },
	}
	
sub _parse_version_safely # stolen from PAUSE's mldistwatch, but refactored
	{
	my( $file ) = @_;
	
	local $/ = "\n";
	local $_; # don't mess with the $_ in the map calling this
	
	return unless open FILE, "<$file";

	my $in_pod = 0;
	my $version;
	while( <FILE> ) 
		{
		chomp;
		$in_pod = /^=(?!cut)/ ? 1 : /^=cut/ ? 0 : $in_pod;
		next if $in_pod || /^\s*#/;

		next unless /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/;
		my( $sigil, $var ) = ( $1, $2 );
		
		$version = _eval_version( $_, $sigil, $var );
		last;
		}
	close FILE;

	return 'undef' unless defined $version;
	
	return $version;
	}

sub _eval_version
	{
	my( $line, $sigil, $var ) = @_;
	
	my $eval = qq{ 
		package ExtUtils::MakeMaker::_version;

		local $sigil$var;
		\$$var=undef; do {
			$line
			}; \$$var
		};
		
	my $version = do {
		local $^W = 0;
		no strict;
		eval( $eval );
		};

	return $version;
	}

sub _path_to_module
	{
	my( $inc, $path ) = @_;
	return if length $path< length $inc;
	
	my $module_path = substr( $path, length $inc );
	$module_path =~ s/\.pm\z//;
	
	# XXX: this is cheating and doesn't handle everything right
	my @dirs = grep { ! /\W/ } File::Spec->splitdir( $module_path );
	shift @dirs;
	
	my $module_name = join "::", @dirs;
	
	return $module_name;
	}

1;

=back

=head1 EXIT VALUES

The script exits with zero if it thinks that everything worked, or a 
positive number if it thinks that something failed. Note, however, that
in some cases it has to divine a failure by the output of things it does
not control. For now, the exit codes are vague:

	1	An unknown error

	2	The was an external problem

	4	There was an internal problem with the script

	8	A module failed to install

=head1 TO DO

* There is initial support for Log4perl if it is available, but I
haven't gone through everything to make the NullLogger work out
correctly if Log4perl is not installed.

* When I capture CPAN.pm output, I need to check for errors and
report them to the user.

=head1 BUGS

* none noted

=head1 SEE ALSO

Most behaviour, including environment variables and configuration,
comes directly from CPAN.pm.

=head1 SOURCE AVAILABILITY

This code is in Github:

	git://github.com/briandfoy/cpan_script.git

=head1 CREDITS

Japheth Cleaver added the bits to allow a forced install (-f).

Jim Brandt suggest and provided the initial implementation for the
up-to-date and Changes features.

Adam Kennedy pointed out that exit() causes problems on Windows
where this script ends up with a .bat extension

=head1 AUTHOR

brian d foy, C<< <bdfoy@cpan.org> >>

=head1 COPYRIGHT

Copyright (c) 2001-2009, brian d foy, All Rights Reserved.

You may redistribute this under the same terms as Perl itself.

=cut
