package IPC::Cmd;

use strict;

BEGIN {

    use constant IS_VMS         => $^O eq 'VMS'                       ? 1 : 0;    
    use constant IS_WIN32       => $^O eq 'MSWin32'                   ? 1 : 0;
    use constant IS_WIN98       => (IS_WIN32 and !Win32::IsWinNT())   ? 1 : 0;
    use constant ALARM_CLASS    => __PACKAGE__ . '::TimeOut';
    use constant SPECIAL_CHARS  => qw[< > | &];
    use constant QUOTE          => do { IS_WIN32 ? q["] : q['] };            

    use Exporter    ();
    use vars        qw[ @ISA $VERSION @EXPORT_OK $VERBOSE $DEBUG
                        $USE_IPC_RUN $USE_IPC_OPEN3 $WARN
                    ];

    $VERSION        = '0.46';
    $VERBOSE        = 0;
    $DEBUG          = 0;
    $WARN           = 1;
    $USE_IPC_RUN    = IS_WIN32 && !IS_WIN98;
    $USE_IPC_OPEN3  = not IS_VMS;

    @ISA            = qw[Exporter];
    @EXPORT_OK      = qw[can_run run QUOTE];
}

require Carp;
use File::Spec;
use Params::Check               qw[check];
use Text::ParseWords            ();             # import ONLY if needed!
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Style => 'gettext';

=pod

=head1 NAME

IPC::Cmd - finding and running system commands made easy

=head1 SYNOPSIS

    use IPC::Cmd qw[can_run run];

    my $full_path = can_run('wget') or warn 'wget is not installed!';

    ### commands can be arrayrefs or strings ###
    my $cmd = "$full_path -b theregister.co.uk";
    my $cmd = [$full_path, '-b', 'theregister.co.uk'];

    ### in scalar context ###
    my $buffer;
    if( scalar run( command => $cmd,
                    verbose => 0,
                    buffer  => \$buffer,
                    timeout => 20 )
    ) {
        print "fetched webpage successfully: $buffer\n";
    }


    ### in list context ###
    my( $success, $error_code, $full_buf, $stdout_buf, $stderr_buf ) =
            run( command => $cmd, verbose => 0 );

    if( $success ) {
        print "this is what the command printed:\n";
        print join "", @$full_buf;
    }

    ### check for features
    print "IPC::Open3 available: "  . IPC::Cmd->can_use_ipc_open3;      
    print "IPC::Run available: "    . IPC::Cmd->can_use_ipc_run;      
    print "Can capture buffer: "    . IPC::Cmd->can_capture_buffer;     

    ### don't have IPC::Cmd be verbose, ie don't print to stdout or
    ### stderr when running commands -- default is '0'
    $IPC::Cmd::VERBOSE = 0;
         

=head1 DESCRIPTION

IPC::Cmd allows you to run commands, interactively if desired,
platform independent but have them still work.

The C<can_run> function can tell you if a certain binary is installed
and if so where, whereas the C<run> function can actually execute any
of the commands you give it and give you a clear return value, as well
as adhere to your verbosity settings.

=head1 CLASS METHODS 

=head2 $ipc_run_version = IPC::Cmd->can_use_ipc_run( [VERBOSE] )

Utility function that tells you if C<IPC::Run> is available. 
If the verbose flag is passed, it will print diagnostic messages
if C<IPC::Run> can not be found or loaded.

=cut


sub can_use_ipc_run     { 
    my $self    = shift;
    my $verbose = shift || 0;
    
    ### ipc::run doesn't run on win98    
    return if IS_WIN98;

    ### if we dont have ipc::run, we obviously can't use it.
    return unless can_load(
                        modules => { 'IPC::Run' => '0.55' },        
                        verbose => ($WARN && $verbose),
                    );
                    
    ### otherwise, we're good to go
    return $IPC::Run::VERSION;                    
}

=head2 $ipc_open3_version = IPC::Cmd->can_use_ipc_open3( [VERBOSE] )

Utility function that tells you if C<IPC::Open3> is available. 
If the verbose flag is passed, it will print diagnostic messages
if C<IPC::Open3> can not be found or loaded.

=cut


sub can_use_ipc_open3   { 
    my $self    = shift;
    my $verbose = shift || 0;

    ### ipc::open3 is not working on VMS becasue of a lack of fork.
    ### XXX todo, win32 also does not have fork, so need to do more research.
    return if IS_VMS;

    ### ipc::open3 works on every non-VMS platform platform, but it can't 
    ### capture buffers on win32 :(
    return unless can_load(
        modules => { map {$_ => '0.0'} qw|IPC::Open3 IO::Select Symbol| },
        verbose => ($WARN && $verbose),
    );
    
    return $IPC::Open3::VERSION;
}

=head2 $bool = IPC::Cmd->can_capture_buffer

Utility function that tells you if C<IPC::Cmd> is capable of
capturing buffers in it's current configuration.

=cut

sub can_capture_buffer {
    my $self    = shift;

    return 1 if $USE_IPC_RUN    && $self->can_use_ipc_run; 
    return 1 if $USE_IPC_OPEN3  && $self->can_use_ipc_open3 && !IS_WIN32; 
    return;
}


=head1 FUNCTIONS

=head2 $path = can_run( PROGRAM );

C<can_run> takes but a single argument: the name of a binary you wish
to locate. C<can_run> works much like the unix binary C<which> or the bash
command C<type>, which scans through your path, looking for the requested
binary .

Unlike C<which> and C<type>, this function is platform independent and
will also work on, for example, Win32.

It will return the full path to the binary you asked for if it was
found, or C<undef> if it was not.

=cut

sub can_run {
    my $command = shift;

    # a lot of VMS executables have a symbol defined
    # check those first
    if ( $^O eq 'VMS' ) {
        require VMS::DCLsym;
        my $syms = VMS::DCLsym->new;
        return $command if scalar $syms->getsym( uc $command );
    }

    require Config;
    require File::Spec;
    require ExtUtils::MakeMaker;

    if( File::Spec->file_name_is_absolute($command) ) {
        return MM->maybe_command($command);

    } else {
        for my $dir (
            (split /\Q$Config::Config{path_sep}\E/, $ENV{PATH}),
            File::Spec->curdir
        ) {           
            my $abs = File::Spec->catfile($dir, $command);
            return $abs if $abs = MM->maybe_command($abs);
        }
    }
}

=head2 $ok | ($ok, $err, $full_buf, $stdout_buff, $stderr_buff) = run( command => COMMAND, [verbose => BOOL, buffer => \$SCALAR, timeout => DIGIT] );

C<run> takes 4 arguments:

=over 4

=item command

This is the command to execute. It may be either a string or an array
reference.
This is a required argument.

See L<CAVEATS> for remarks on how commands are parsed and their
limitations.

=item verbose

This controls whether all output of a command should also be printed
to STDOUT/STDERR or should only be trapped in buffers (NOTE: buffers
require C<IPC::Run> to be installed or your system able to work with
C<IPC::Open3>).

It will default to the global setting of C<$IPC::Cmd::VERBOSE>,
which by default is 0.

=item buffer

This will hold all the output of a command. It needs to be a reference
to a scalar.
Note that this will hold both the STDOUT and STDERR messages, and you
have no way of telling which is which.
If you require this distinction, run the C<run> command in list context
and inspect the individual buffers.

Of course, this requires that the underlying call supports buffers. See
the note on buffers right above.

=item timeout

Sets the maximum time the command is allowed to run before aborting,
using the built-in C<alarm()> call. If the timeout is triggered, the
C<errorcode> in the return value will be set to an object of the 
C<IPC::Cmd::TimeOut> class. See the C<errorcode> section below for
details.

Defaults to C<0>, meaning no timeout is set.

=back

C<run> will return a simple C<true> or C<false> when called in scalar
context.
In list context, you will be returned a list of the following items:

=over 4

=item success

A simple boolean indicating if the command executed without errors or
not.

=item error message

If the first element of the return value (success) was 0, then some
error occurred. This second element is the error message the command
you requested exited with, if available. This is generally a pretty 
printed value of C<$?> or C<$@>. See C<perldoc perlvar> for details on 
what they can contain.
If the error was a timeout, the C<error message> will be prefixed with
the string C<IPC::Cmd::TimeOut>, the timeout class.

=item full_buffer

This is an arrayreference containing all the output the command
generated.
Note that buffers are only available if you have C<IPC::Run> installed,
or if your system is able to work with C<IPC::Open3> -- See below).
This element will be C<undef> if this is not the case.

=item out_buffer

This is an arrayreference containing all the output sent to STDOUT the
command generated.
Note that buffers are only available if you have C<IPC::Run> installed,
or if your system is able to work with C<IPC::Open3> -- See below).
This element will be C<undef> if this is not the case.

=item error_buffer

This is an arrayreference containing all the output sent to STDERR the
command generated.
Note that buffers are only available if you have C<IPC::Run> installed,
or if your system is able to work with C<IPC::Open3> -- See below).
This element will be C<undef> if this is not the case.

=back

See the C<HOW IT WORKS> Section below to see how C<IPC::Cmd> decides
what modules or function calls to use when issuing a command.

=cut

{   my @acc = qw[ok error _fds];
    
    ### autogenerate accessors ###
    for my $key ( @acc ) {
        no strict 'refs';
        *{__PACKAGE__."::$key"} = sub {
            $_[0]->{$key} = $_[1] if @_ > 1;
            return $_[0]->{$key};
        }
    }
}

sub run {
    ### container to store things in
    my $self = bless {}, __PACKAGE__;

    my %hash = @_;
    
    ### if the user didn't provide a buffer, we'll store it here.
    my $def_buf = '';
    
    my($verbose,$cmd,$buffer,$timeout);
    my $tmpl = {
        verbose => { default  => $VERBOSE,  store => \$verbose },
        buffer  => { default  => \$def_buf, store => \$buffer },
        command => { required => 1,         store => \$cmd,
                     allow    => sub { !ref($_[0]) or ref($_[0]) eq 'ARRAY' }, 
        },
        timeout => { default  => 0,         store => \$timeout },                    
    };
    
    unless( check( $tmpl, \%hash, $VERBOSE ) ) {
        Carp::carp( loc( "Could not validate input: %1",
                         Params::Check->last_error ) );
        return;
    };        

    $cmd = _quote_args_vms( $cmd ) if IS_VMS;

    ### strip any empty elements from $cmd if present
    $cmd = [ grep { length && defined } @$cmd ] if ref $cmd;

    my $pp_cmd = (ref $cmd ? "@$cmd" : $cmd);
    print loc("Running [%1]...\n", $pp_cmd ) if $verbose;

    ### did the user pass us a buffer to fill or not? if so, set this
    ### flag so we know what is expected of us
    ### XXX this is now being ignored. in the future, we could add diagnostic
    ### messages based on this logic
    #my $user_provided_buffer = $buffer == \$def_buf ? 0 : 1;
    
    ### buffers that are to be captured
    my( @buffer, @buff_err, @buff_out );

    ### capture STDOUT
    my $_out_handler = sub {
        my $buf = shift;
        return unless defined $buf;
       
        print STDOUT $buf if $verbose;
        push @buffer,   $buf;
        push @buff_out, $buf;
    };
    
    ### capture STDERR
    my $_err_handler = sub {
        my $buf = shift;
        return unless defined $buf;
        
        print STDERR $buf if $verbose;
        push @buffer,   $buf;
        push @buff_err, $buf;
    };
    

    ### flag to indicate we have a buffer captured
    my $have_buffer = $self->can_capture_buffer ? 1 : 0;
    
    ### flag indicating if the subcall went ok
    my $ok;
    
    ### dont look at previous errors:
    local $?;  
    local $@;
    local $!;

    ### we might be having a timeout set
    eval {   
        local $SIG{ALRM} = sub { die bless sub { 
            ALARM_CLASS . 
            qq[: Command '$pp_cmd' aborted by alarm after $timeout seconds]
        }, ALARM_CLASS } if $timeout;
        alarm $timeout || 0;
    
        ### IPC::Run is first choice if $USE_IPC_RUN is set.
        if( $USE_IPC_RUN and $self->can_use_ipc_run( 1 ) ) {
            ### ipc::run handlers needs the command as a string or an array ref
    
            $self->_debug( "# Using IPC::Run. Have buffer: $have_buffer" )
                if $DEBUG;
                
            $ok = $self->_ipc_run( $cmd, $_out_handler, $_err_handler );
    
        ### since IPC::Open3 works on all platforms, and just fails on
        ### win32 for capturing buffers, do that ideally
        } elsif ( $USE_IPC_OPEN3 and $self->can_use_ipc_open3( 1 ) ) {
    
            $self->_debug("# Using IPC::Open3. Have buffer: $have_buffer")
                if $DEBUG;
    
            ### in case there are pipes in there;
            ### IPC::Open3 will call exec and exec will do the right thing 
            $ok = $self->_open3_run( 
                                    $cmd, $_out_handler, $_err_handler, $verbose 
                                );
            
        ### if we are allowed to run verbose, just dispatch the system command
        } else {
            $self->_debug( "# Using system(). Have buffer: $have_buffer" )
                if $DEBUG;
            $ok = $self->_system_run( $cmd, $verbose );
        }
        
        alarm 0;
    };
   
    ### restore STDIN after duping, or STDIN will be closed for
    ### this current perl process!   
    $self->__reopen_fds( @{ $self->_fds} ) if $self->_fds;
    
    my $err;
    unless( $ok ) {
        ### alarm happened
        if ( $@ and ref $@ and $@->isa( ALARM_CLASS ) ) {
            $err = $@->();  # the error code is an expired alarm

        ### another error happened, set by the dispatchub
        } else {
            $err = $self->error;
        }
    }
    
    ### fill the buffer;
    $$buffer = join '', @buffer if @buffer;
    
    ### return a list of flags and buffers (if available) in list
    ### context, or just a simple 'ok' in scalar
    return wantarray
                ? $have_buffer
                    ? ($ok, $err, \@buffer, \@buff_out, \@buff_err)
                    : ($ok, $err )
                : $ok
    
    
}

sub _open3_run { 
    my $self            = shift;
    my $cmd             = shift;
    my $_out_handler    = shift;
    my $_err_handler    = shift;
    my $verbose         = shift || 0;

    ### Following code are adapted from Friar 'abstracts' in the
    ### Perl Monastery (http://www.perlmonks.org/index.pl?node_id=151886).
    ### XXX that code didn't work.
    ### we now use the following code, thanks to theorbtwo

    ### define them beforehand, so we always have defined FH's
    ### to read from.
    use Symbol;    
    my $kidout      = Symbol::gensym();
    my $kiderror    = Symbol::gensym();

    ### Dup the filehandle so we can pass 'our' STDIN to the
    ### child process. This stops us from having to pump input
    ### from ourselves to the childprocess. However, we will need
    ### to revive the FH afterwards, as IPC::Open3 closes it.
    ### We'll do the same for STDOUT and STDERR. It works without
    ### duping them on non-unix derivatives, but not on win32.
    my @fds_to_dup = ( IS_WIN32 && !$verbose 
                            ? qw[STDIN STDOUT STDERR] 
                            : qw[STDIN]
                        );
    $self->_fds( \@fds_to_dup );
    $self->__dup_fds( @fds_to_dup );
    
    ### pipes have to come in a quoted string, and that clashes with
    ### whitespace. This sub fixes up such commands so they run properly
    $cmd = $self->__fix_cmd_whitespace_and_special_chars( $cmd );
        
    ### dont stringify @$cmd, so spaces in filenames/paths are
    ### treated properly
    my $pid = eval { 
        IPC::Open3::open3(
                    '<&STDIN',
                    (IS_WIN32 ? '>&STDOUT' : $kidout),
                    (IS_WIN32 ? '>&STDERR' : $kiderror),
                    ( ref $cmd ? @$cmd : $cmd ),
                );
    };
    
    ### open3 error occurred 
    if( $@ and $@ =~ /^open3:/ ) {
        $self->ok( 0 );
        $self->error( $@ );
        return;
    };

    ### use OUR stdin, not $kidin. Somehow,
    ### we never get the input.. so jump through
    ### some hoops to do it :(
    my $selector = IO::Select->new(
                        (IS_WIN32 ? \*STDERR : $kiderror), 
                        \*STDIN,   
                        (IS_WIN32 ? \*STDOUT : $kidout)     
                    );              

    STDOUT->autoflush(1);   STDERR->autoflush(1);   STDIN->autoflush(1);
    $kidout->autoflush(1)   if UNIVERSAL::can($kidout,   'autoflush');
    $kiderror->autoflush(1) if UNIVERSAL::can($kiderror, 'autoflush');

    ### add an epxlicit break statement
    ### code courtesy of theorbtwo from #london.pm
    my $stdout_done = 0;
    my $stderr_done = 0;
    OUTER: while ( my @ready = $selector->can_read ) {

        for my $h ( @ready ) {
            my $buf;
            
            ### $len is the amount of bytes read
            my $len = sysread( $h, $buf, 4096 );    # try to read 4096 bytes
            
            ### see perldoc -f sysread: it returns undef on error,
            ### so bail out.
            if( not defined $len ) {
                warn(loc("Error reading from process: %1", $!));
                last OUTER;
            }

            ### check for $len. it may be 0, at which point we're
            ### done reading, so don't try to process it.
            ### if we would print anyway, we'd provide bogus information
            $_out_handler->( "$buf" ) if $len && $h == $kidout;
            $_err_handler->( "$buf" ) if $len && $h == $kiderror;

            ### Wait till child process is done printing to both
            ### stdout and stderr.
            $stdout_done = 1 if $h == $kidout   and $len == 0;
            $stderr_done = 1 if $h == $kiderror and $len == 0;
            last OUTER if ($stdout_done && $stderr_done);
        }
    }

    waitpid $pid, 0; # wait for it to die

    ### restore STDIN after duping, or STDIN will be closed for
    ### this current perl process!
    ### done in the parent call now
    # $self->__reopen_fds( @fds_to_dup );
    
    ### some error occurred
    if( $? ) {
        $self->error( $self->_pp_child_error( $cmd, $? ) );   
        $self->ok( 0 );
        return;
    } else {
        return $self->ok( 1 );
    }
}

### text::parsewords::shellwordss() uses unix semantics. that will break
### on win32
{   my $parse_sub = IS_WIN32 
                        ? __PACKAGE__->can('_split_like_shell_win32')
                        : Text::ParseWords->can('shellwords');

    sub _ipc_run {  
        my $self            = shift;
        my $cmd             = shift;
        my $_out_handler    = shift;
        my $_err_handler    = shift;
        
        STDOUT->autoflush(1); STDERR->autoflush(1);

        ### a command like:
        # [
        #     '/usr/bin/gzip',
        #     '-cdf',
        #     '/Users/kane/sources/p4/other/archive-extract/t/src/x.tgz',
        #     '|',
        #     '/usr/bin/tar',
        #     '-tf -'
        # ]
        ### needs to become:
        # [
        #     ['/usr/bin/gzip', '-cdf',
        #       '/Users/kane/sources/p4/other/archive-extract/t/src/x.tgz']
        #     '|',
        #     ['/usr/bin/tar', '-tf -']
        # ]

    
        my @command; 
        my $special_chars;
    
        my $re = do { my $x = join '', SPECIAL_CHARS; qr/([$x])/ };
        if( ref $cmd ) {
            my $aref = [];
            for my $item (@$cmd) {
                if( $item =~ $re ) {
                    push @command, $aref, $item;
                    $aref = [];
                    $special_chars .= $1;
                } else {
                    push @$aref, $item;
                }
            }
            push @command, $aref;
        } else {
            @command = map { if( $_ =~ $re ) {
                                $special_chars .= $1; $_;
                             } else {
#                                [ split /\s+/ ]
                                 [ map { m/[ ]/ ? qq{'$_'} : $_ } $parse_sub->($_) ]
                             }
                        } split( /\s*$re\s*/, $cmd );
        }

        ### if there's a pipe in the command, *STDIN needs to 
        ### be inserted *BEFORE* the pipe, to work on win32
        ### this also works on *nix, so we should do it when possible
        ### this should *also* work on multiple pipes in the command
        ### if there's no pipe in the command, append STDIN to the back
        ### of the command instead.
        ### XXX seems IPC::Run works it out for itself if you just
        ### dont pass STDIN at all.
        #     if( $special_chars and $special_chars =~ /\|/ ) {
        #         ### only add STDIN the first time..
        #         my $i;
        #         @command = map { ($_ eq '|' && not $i++) 
        #                             ? ( \*STDIN, $_ ) 
        #                             : $_ 
        #                         } @command; 
        #     } else {
        #         push @command, \*STDIN;
        #     }
  
        # \*STDIN is already included in the @command, see a few lines up
        my $ok = eval { IPC::Run::run(   @command, 
                                fileno(STDOUT).'>',
                                $_out_handler,
                                fileno(STDERR).'>',
                                $_err_handler
                            )
                        };

        ### all is well
        if( $ok ) {
            return $self->ok( $ok );

        ### some error occurred
        } else {
            $self->ok( 0 );

            ### if the eval fails due to an exception, deal with it
            ### unless it's an alarm 
            if( $@ and not UNIVERSAL::isa( $@, ALARM_CLASS ) ) {        
                $self->error( $@ );

            ### if it *is* an alarm, propagate        
            } elsif( $@ ) {
                die $@;

            ### some error in the sub command
            } else {
                $self->error( $self->_pp_child_error( $cmd, $? ) );
            }
    
            return;
        }
    }
}

sub _system_run { 
    my $self    = shift;
    my $cmd     = shift;
    my $verbose = shift || 0;

    ### pipes have to come in a quoted string, and that clashes with
    ### whitespace. This sub fixes up such commands so they run properly
    $cmd = $self->__fix_cmd_whitespace_and_special_chars( $cmd );

    my @fds_to_dup = $verbose ? () : qw[STDOUT STDERR];
    $self->_fds( \@fds_to_dup );
    $self->__dup_fds( @fds_to_dup );

    ### system returns 'true' on failure -- the exit code of the cmd
    $self->ok( 1 );
    system( ref $cmd ? @$cmd : $cmd ) == 0 or do {
        $self->error( $self->_pp_child_error( $cmd, $? ) );
        $self->ok( 0 );
    };

    ### done in the parent call now
    #$self->__reopen_fds( @fds_to_dup );

    return unless $self->ok;
    return $self->ok;
}

{   my %sc_lookup = map { $_ => $_ } SPECIAL_CHARS;


    sub __fix_cmd_whitespace_and_special_chars {
        my $self = shift;
        my $cmd  = shift;

        ### command has a special char in it
        if( ref $cmd and grep { $sc_lookup{$_} } @$cmd ) {
            
            ### since we have special chars, we have to quote white space
            ### this *may* conflict with the parsing :(
            my $fixed;
            my @cmd = map { / / ? do { $fixed++; QUOTE.$_.QUOTE } : $_ } @$cmd;
            
            $self->_debug( "# Quoted $fixed arguments containing whitespace" )
                    if $DEBUG && $fixed;
            
            ### stringify it, so the special char isn't escaped as argument
            ### to the program
            $cmd = join ' ', @cmd;
        }

        return $cmd;
    }
}

### Command-line arguments (but not the command itself) must be quoted
### to ensure case preservation. Borrowed from Module::Build with adaptations.
### Patch for this supplied by Craig Berry, see RT #46288: [PATCH] Add argument
### quoting for run() on VMS
sub _quote_args_vms {
  ### Returns a command string with proper quoting so that the subprocess
  ### sees this same list of args, or if we get a single arg that is an
  ### array reference, quote the elements of it (except for the first)
  ### and return the reference.
  my @args = @_;
  my $got_arrayref = (scalar(@args) == 1
                      && UNIVERSAL::isa($args[0], 'ARRAY'))
                   ? 1
                   : 0;

  @args = split(/\s+/, $args[0]) unless $got_arrayref || scalar(@args) > 1;

  my $cmd = $got_arrayref ? shift @{$args[0]} : shift @args;

  ### Do not quote qualifiers that begin with '/' or previously quoted args.
  map { if (/^[^\/\"]/) {
          $_ =~ s/\"/""/g;     # escape C<"> by doubling
          $_ = q(").$_.q(");
        }
  }
    ($got_arrayref ? @{$args[0]}
                   : @args
    );

  $got_arrayref ? unshift(@{$args[0]}, $cmd) : unshift(@args, $cmd);

  return $got_arrayref ? $args[0]
                       : join(' ', @args);
}


### XXX this is cribbed STRAIGHT from M::B 0.30 here:
### http://search.cpan.org/src/KWILLIAMS/Module-Build-0.30/lib/Module/Build/Platform/Windows.pm:split_like_shell
### XXX this *should* be integrated into text::parsewords
sub _split_like_shell_win32 {
  # As it turns out, Windows command-parsing is very different from
  # Unix command-parsing.  Double-quotes mean different things,
  # backslashes don't necessarily mean escapes, and so on.  So we
  # can't use Text::ParseWords::shellwords() to break a command string
  # into words.  The algorithm below was bashed out by Randy and Ken
  # (mostly Randy), and there are a lot of regression tests, so we
  # should feel free to adjust if desired.
  
  local $_ = shift;
  
  my @argv;
  return @argv unless defined() && length();
  
  my $arg = '';
  my( $i, $quote_mode ) = ( 0, 0 );
  
  while ( $i < length() ) {
    
    my $ch      = substr( $_, $i  , 1 );
    my $next_ch = substr( $_, $i+1, 1 );
    
    if ( $ch eq '\\' && $next_ch eq '"' ) {
      $arg .= '"';
      $i++;
    } elsif ( $ch eq '\\' && $next_ch eq '\\' ) {
      $arg .= '\\';
      $i++;
    } elsif ( $ch eq '"' && $next_ch eq '"' && $quote_mode ) {
      $quote_mode = !$quote_mode;
      $arg .= '"';
      $i++;
    } elsif ( $ch eq '"' && $next_ch eq '"' && !$quote_mode &&
          ( $i + 2 == length()  ||
        substr( $_, $i + 2, 1 ) eq ' ' )
        ) { # for cases like: a"" => [ 'a' ]
      push( @argv, $arg );
      $arg = '';
      $i += 2;
    } elsif ( $ch eq '"' ) {
      $quote_mode = !$quote_mode;
    } elsif ( $ch eq ' ' && !$quote_mode ) {
      push( @argv, $arg ) if $arg;
      $arg = '';
      ++$i while substr( $_, $i + 1, 1 ) eq ' ';
    } else {
      $arg .= $ch;
    }
    
    $i++;
  }
  
  push( @argv, $arg ) if defined( $arg ) && length( $arg );
  return @argv;
}



{   use File::Spec;
    use Symbol;

    my %Map = (
        STDOUT => [qw|>&|, \*STDOUT, Symbol::gensym() ],
        STDERR => [qw|>&|, \*STDERR, Symbol::gensym() ],
        STDIN  => [qw|<&|, \*STDIN,  Symbol::gensym() ],
    );

    ### dups FDs and stores them in a cache
    sub __dup_fds {
        my $self    = shift;
        my @fds     = @_;

        __PACKAGE__->_debug( "# Closing the following fds: @fds" ) if $DEBUG;

        for my $name ( @fds ) {
            my($redir, $fh, $glob) = @{$Map{$name}} or (
                Carp::carp(loc("No such FD: '%1'", $name)), next );
            
            ### MUST use the 2-arg version of open for dup'ing for 
            ### 5.6.x compatibilty. 5.8.x can use 3-arg open
            ### see perldoc5.6.2 -f open for details            
            open $glob, $redir . fileno($fh) or (
                        Carp::carp(loc("Could not dup '$name': %1", $!)),
                        return
                    );        
                
            ### we should re-open this filehandle right now, not
            ### just dup it
            ### Use 2-arg version of open, as 5.5.x doesn't support
            ### 3-arg version =/
            if( $redir eq '>&' ) {
                open( $fh, '>' . File::Spec->devnull ) or (
                    Carp::carp(loc("Could not reopen '$name': %1", $!)),
                    return
                );
            }
        }
        
        return 1;
    }

    ### reopens FDs from the cache    
    sub __reopen_fds {
        my $self    = shift;
        my @fds     = @_;

        __PACKAGE__->_debug( "# Reopening the following fds: @fds" ) if $DEBUG;

        for my $name ( @fds ) {
            my($redir, $fh, $glob) = @{$Map{$name}} or (
                Carp::carp(loc("No such FD: '%1'", $name)), next );

            ### MUST use the 2-arg version of open for dup'ing for 
            ### 5.6.x compatibilty. 5.8.x can use 3-arg open
            ### see perldoc5.6.2 -f open for details
            open( $fh, $redir . fileno($glob) ) or (
                    Carp::carp(loc("Could not restore '$name': %1", $!)),
                    return
                ); 
           
            ### close this FD, we're not using it anymore
            close $glob;                
        }                
        return 1;                
    
    }
}    

sub _debug {
    my $self    = shift;
    my $msg     = shift or return;
    my $level   = shift || 0;
    
    local $Carp::CarpLevel += $level;
    Carp::carp($msg);
    
    return 1;
}

sub _pp_child_error {
    my $self    = shift;
    my $cmd     = shift or return;
    my $ce      = shift or return;
    my $pp_cmd  = ref $cmd ? "@$cmd" : $cmd;
    
            
    my $str;
    if( $ce == -1 ) {
        ### Include $! in the error message, so that the user can
        ### see 'No such file or directory' versus 'Permission denied'
        ### versus 'Cannot fork' or whatever the cause was.
        $str = "Failed to execute '$pp_cmd': $!";

    } elsif ( $ce & 127 ) {       
        ### some signal
        $str = loc( "'%1' died with signal %d, %s coredump\n",
               $pp_cmd, ($ce & 127), ($ce & 128) ? 'with' : 'without');

    } else {
        ### Otherwise, the command run but gave error status.
        $str = "'$pp_cmd' exited with value " . ($ce >> 8);
    }
  
    $self->_debug( "# Child error '$ce' translated to: $str" ) if $DEBUG;
    
    return $str;
}

1;

=head2 $q = QUOTE

Returns the character used for quoting strings on this platform. This is
usually a C<'> (single quote) on most systems, but some systems use different
quotes. For example, C<Win32> uses C<"> (double quote). 

You can use it as follows:

  use IPC::Cmd qw[run QUOTE];
  my $cmd = q[echo ] . QUOTE . q[foo bar] . QUOTE;

This makes sure that C<foo bar> is treated as a string, rather than two
seperate arguments to the C<echo> function.

__END__

=head1 HOW IT WORKS

C<run> will try to execute your command using the following logic:

=over 4

=item *

If you have C<IPC::Run> installed, and the variable C<$IPC::Cmd::USE_IPC_RUN>
is set to true (See the C<GLOBAL VARIABLES> Section) use that to execute 
the command. You will have the full output available in buffers, interactive commands are sure to work  and you are guaranteed to have your verbosity
settings honored cleanly.

=item *

Otherwise, if the variable C<$IPC::Cmd::USE_IPC_OPEN3> is set to true 
(See the C<GLOBAL VARIABLES> Section), try to execute the command using
C<IPC::Open3>. Buffers will be available on all platforms except C<Win32>,
interactive commands will still execute cleanly, and also your verbosity
settings will be adhered to nicely;

=item *

Otherwise, if you have the verbose argument set to true, we fall back
to a simple system() call. We cannot capture any buffers, but
interactive commands will still work.

=item *

Otherwise we will try and temporarily redirect STDERR and STDOUT, do a
system() call with your command and then re-open STDERR and STDOUT.
This is the method of last resort and will still allow you to execute
your commands cleanly. However, no buffers will be available.

=back

=head1 Global Variables

The behaviour of IPC::Cmd can be altered by changing the following
global variables:

=head2 $IPC::Cmd::VERBOSE

This controls whether IPC::Cmd will print any output from the
commands to the screen or not. The default is 0;

=head2 $IPC::Cmd::USE_IPC_RUN

This variable controls whether IPC::Cmd will try to use L<IPC::Run>
when available and suitable. Defaults to true if you are on C<Win32>.

=head2 $IPC::Cmd::USE_IPC_OPEN3

This variable controls whether IPC::Cmd will try to use L<IPC::Open3>
when available and suitable. Defaults to true.

=head2 $IPC::Cmd::WARN

This variable controls whether run time warnings should be issued, like
the failure to load an C<IPC::*> module you explicitly requested.

Defaults to true. Turn this off at your own risk.

=head1 Caveats

=over 4

=item Whitespace and IPC::Open3 / system()

When using C<IPC::Open3> or C<system>, if you provide a string as the
C<command> argument, it is assumed to be appropriately escaped. You can
use the C<QUOTE> constant to use as a portable quote character (see above).
However, if you provide and C<Array Reference>, special rules apply:

If your command contains C<Special Characters> (< > | &), it will
be internally stringified before executing the command, to avoid that these
special characters are escaped and passed as arguments instead of retaining
their special meaning.

However, if the command contained arguments that contained whitespace, 
stringifying the command would loose the significance of the whitespace.
Therefor, C<IPC::Cmd> will quote any arguments containing whitespace in your
command if the command is passed as an arrayref and contains special characters.

=item Whitespace and IPC::Run

When using C<IPC::Run>, if you provide a string as the C<command> argument, 
the string will be split on whitespace to determine the individual elements 
of your command. Although this will usually just Do What You Mean, it may
break if you have files or commands with whitespace in them.

If you do not wish this to happen, you should provide an array
reference, where all parts of your command are already separated out.
Note however, if there's extra or spurious whitespace in these parts,
the parser or underlying code may not interpret it correctly, and
cause an error.

Example:
The following code

    gzip -cdf foo.tar.gz | tar -xf -

should either be passed as

    "gzip -cdf foo.tar.gz | tar -xf -"

or as

    ['gzip', '-cdf', 'foo.tar.gz', '|', 'tar', '-xf', '-']

But take care not to pass it as, for example

    ['gzip -cdf foo.tar.gz', '|', 'tar -xf -']

Since this will lead to issues as described above.


=item IO Redirect

Currently it is too complicated to parse your command for IO
Redirections. For capturing STDOUT or STDERR there is a work around
however, since you can just inspect your buffers for the contents.

=item Interleaving STDOUT/STDERR

Neither IPC::Run nor IPC::Open3 can interleave STDOUT and STDERR. For short
bursts of output from a program, ie this sample:

    for ( 1..4 ) {
        $_ % 2 ? print STDOUT $_ : print STDERR $_;
    }

IPC::[Run|Open3] will first read all of STDOUT, then all of STDERR, meaning 
the output looks like 1 line on each, namely '13' on STDOUT and '24' on STDERR.

It should have been 1, 2, 3, 4.

This has been recorded in L<rt.cpan.org> as bug #37532: Unable to interleave
STDOUT and STDERR

=back

=head1 See Also

C<IPC::Run>, C<IPC::Open3>

=head1 ACKNOWLEDGEMENTS

Thanks to James Mastros and Martijn van der Streek for their
help in getting IPC::Open3 to behave nicely.

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-ipc-cmd@rt.cpan.orgE<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=cut
