package DB;

# Debugger for Perl 5.00x; perl5db.pl patch level:
$VERSION = 1.19;
$header  = "perl5db.pl version $VERSION";

# It is crucial that there is no lexicals in scope of `eval ""' down below
sub eval {
    # 'my' would make it visible from user code
    #    but so does local! --tchrist  [... into @DB::res, not @res. IZ]
    local @res;
    {
	local $otrace = $trace;
	local $osingle = $single;
	local $od = $^D;
	{ ($evalarg) = $evalarg =~ /(.*)/s; }
	@res = eval "$usercontext $evalarg;\n"; # '\n' for nice recursive debug
	$trace = $otrace;
	$single = $osingle;
	$^D = $od;
    }
    my $at = $@;
    local $saved[0];		# Preserve the old value of $@
    eval { &DB::save };
    if ($at) {
	local $\ = '';
	print $OUT $at;
    } elsif ($onetimeDump) {
      if ($onetimeDump eq 'dump')  {
        local $option{dumpDepth} = $onetimedumpDepth 
          if defined $onetimedumpDepth;
	dumpit($OUT, \@res);
      } elsif ($onetimeDump eq 'methods') {
	methods($res[0]) ;
      }
    }
    @res;
}

# After this point it is safe to introduce lexicals
# However, one should not overdo it: leave as much control from outside as possible
#
# This file is automatically included if you do perl -d.
# It's probably not useful to include this yourself.
#
# Before venturing further into these twisty passages, it is 
# wise to read the perldebguts man page or risk the ire of dragons.
#
# Perl supplies the values for %sub.  It effectively inserts
# a &DB::DB(); in front of every place that can have a
# breakpoint. Instead of a subroutine call it calls &DB::sub with
# $DB::sub being the called subroutine. It also inserts a BEGIN
# {require 'perl5db.pl'} before the first line.
#
# After each `require'd file is compiled, but before it is executed, a
# call to DB::postponed($main::{'_<'.$filename}) is emulated. Here the
# $filename is the expanded name of the `require'd file (as found as
# value of %INC).
#
# Additional services from Perl interpreter:
#
# if caller() is called from the package DB, it provides some
# additional data.
#
# The array @{$main::{'_<'.$filename}} (herein called @dbline) is the
# line-by-line contents of $filename.
#
# The hash %{'_<'.$filename} (herein called %dbline) contains
# breakpoints and action (it is keyed by line number), and individual
# entries are settable (as opposed to the whole hash). Only true/false
# is important to the interpreter, though the values used by
# perl5db.pl have the form "$break_condition\0$action". Values are
# magical in numeric context.
#
# The scalar ${'_<'.$filename} contains $filename.
#
# Note that no subroutine call is possible until &DB::sub is defined
# (for subroutines defined outside of the package DB). In fact the same is
# true if $deep is not defined.
#
# $Log:	perldb.pl,v $

#
# At start reads $rcfile that may set important options.  This file
# may define a subroutine &afterinit that will be executed after the
# debugger is initialized.
#
# After $rcfile is read reads environment variable PERLDB_OPTS and parses
# it as a rest of `O ...' line in debugger prompt.
#
# The options that can be specified only at startup:
# [To set in $rcfile, call &parse_options("optionName=new_value").]
#
# TTY  - the TTY to use for debugging i/o.
#
# noTTY - if set, goes in NonStop mode.  On interrupt if TTY is not set
# uses the value of noTTY or "/tmp/perldbtty$$" to find TTY using
# Term::Rendezvous.  Current variant is to have the name of TTY in this
# file.
#
# ReadLine - If false, dummy ReadLine is used, so you can debug
# ReadLine applications.
#
# NonStop - if true, no i/o is performed until interrupt.
#
# LineInfo - file or pipe to print line number info to.  If it is a
# pipe, a short "emacs like" message is used.
#
# RemotePort - host:port to connect to on remote host for remote debugging.
#
# Example $rcfile: (delete leading hashes!)
#
# &parse_options("NonStop=1 LineInfo=db.out");
# sub afterinit { $trace = 1; }
#
# The script will run without human intervention, putting trace
# information into db.out.  (If you interrupt it, you would better
# reset LineInfo to something "interactive"!)
#
##################################################################

# Enhanced by ilya@math.ohio-state.edu (Ilya Zakharevich)

# modified Perl debugger, to be run from Emacs in perldb-mode
# Ray Lischner (uunet!mntgfx!lisch) as of 5 Nov 1990
# Johan Vromans -- upgrade to 4.0 pl 10
# Ilya Zakharevich -- patches after 5.001 (and some before ;-)

# Changelog:

# A lot of things changed after 0.94. First of all, core now informs
# debugger about entry into XSUBs, overloaded operators, tied operations,
# BEGIN and END. Handy with `O f=2'.

# This can make debugger a little bit too verbose, please be patient
# and report your problems promptly.

# Now the option frame has 3 values: 0,1,2.

# Note that if DESTROY returns a reference to the object (or object),
# the deletion of data may be postponed until the next function call,
# due to the need to examine the return value.

# Changes: 0.95: `v' command shows versions.
# Changes: 0.96: `v' command shows version of readline.
#	primitive completion works (dynamic variables, subs for `b' and `l',
#		options). Can `p %var'
#	Better help (`h <' now works). New commands <<, >>, {, {{.
#	{dump|print}_trace() coded (to be able to do it from <<cmd).
#	`c sub' documented.
#	At last enough magic combined to stop after the end of debuggee.
#	!! should work now (thanks to Emacs bracket matching an extra
#	`]' in a regexp is caught).
#	`L', `D' and `A' span files now (as documented).
#	Breakpoints in `require'd code are possible (used in `R').
#	Some additional words on internal work of debugger.
#	`b load filename' implemented.
#	`b postpone subr' implemented.
#	now only `q' exits debugger (overwritable on $inhibit_exit).
#	When restarting debugger breakpoints/actions persist.
#     Buglet: When restarting debugger only one breakpoint/action per 
#		autoloaded function persists.
# Changes: 0.97: NonStop will not stop in at_exit().
#	Option AutoTrace implemented.
#	Trace printed differently if frames are printed too.
#	new `inhibitExit' option.
#	printing of a very long statement interruptible.
# Changes: 0.98: New command `m' for printing possible methods
#	'l -' is a synonym for `-'.
#	Cosmetic bugs in printing stack trace.
#	`frame' & 8 to print "expanded args" in stack trace.
#	Can list/break in imported subs.
#	new `maxTraceLen' option.
#	frame & 4 and frame & 8 granted.
#	new command `m'
#	nonstoppable lines do not have `:' near the line number.
#	`b compile subname' implemented.
#	Will not use $` any more.
#	`-' behaves sane now.
# Changes: 0.99: Completion for `f', `m'.
#	`m' will remove duplicate names instead of duplicate functions.
#	`b load' strips trailing whitespace.
#	completion ignores leading `|'; takes into account current package
#	when completing a subroutine name (same for `l').
# Changes: 1.07: Many fixed by tchrist 13-March-2000
#   BUG FIXES:
#   + Added bare minimal security checks on perldb rc files, plus
#     comments on what else is needed.
#   + Fixed the ornaments that made "|h" completely unusable.
#     They are not used in print_help if they will hurt.  Strip pod
#     if we're paging to less.
#   + Fixed mis-formatting of help messages caused by ornaments
#     to restore Larry's original formatting.  
#   + Fixed many other formatting errors.  The code is still suboptimal, 
#     and needs a lot of work at restructuring.  It's also misindented
#     in many places.
#   + Fixed bug where trying to look at an option like your pager
#     shows "1".  
#   + Fixed some $? processing.  Note: if you use csh or tcsh, you will
#     lose.  You should consider shell escapes not using their shell,
#     or else not caring about detailed status.  This should really be
#     unified into one place, too.
#   + Fixed bug where invisible trailing whitespace on commands hoses you,
#     tricking Perl into thinking you weren't calling a debugger command!
#   + Fixed bug where leading whitespace on commands hoses you.  (One
#     suggests a leading semicolon or any other irrelevant non-whitespace
#     to indicate literal Perl code.)
#   + Fixed bugs that ate warnings due to wrong selected handle.
#   + Fixed a precedence bug on signal stuff.
#   + Fixed some unseemly wording.
#   + Fixed bug in help command trying to call perl method code.
#   + Fixed to call dumpvar from exception handler.  SIGPIPE killed us.
#   ENHANCEMENTS:
#   + Added some comments.  This code is still nasty spaghetti.
#   + Added message if you clear your pre/post command stacks which was
#     very easy to do if you just typed a bare >, <, or {.  (A command
#     without an argument should *never* be a destructive action; this
#     API is fundamentally screwed up; likewise option setting, which
#     is equally buggered.)
#   + Added command stack dump on argument of "?" for >, <, or {.
#   + Added a semi-built-in doc viewer command that calls man with the
#     proper %Config::Config path (and thus gets caching, man -k, etc),
#     or else perldoc on obstreperous platforms.
#   + Added to and rearranged the help information.
#   + Detected apparent misuse of { ... } to declare a block; this used
#     to work but now is a command, and mysteriously gave no complaint.
#
# Changes: 1.08: Apr 25, 2001  Jon Eveland <jweveland@yahoo.com>
#   BUG FIX:
#   + This patch to perl5db.pl cleans up formatting issues on the help
#     summary (h h) screen in the debugger.  Mostly columnar alignment
#     issues, plus converted the printed text to use all spaces, since
#     tabs don't seem to help much here.
#
# Changes: 1.09: May 19, 2001  Ilya Zakharevich <ilya@math.ohio-state.edu>
#   0) Minor bugs corrected;
#   a) Support for auto-creation of new TTY window on startup, either
#      unconditionally, or if started as a kid of another debugger session;
#   b) New `O'ption CreateTTY
#       I<CreateTTY>       bits control attempts to create a new TTY on events:
#                          1: on fork()   2: debugger is started inside debugger
#                          4: on startup
#   c) Code to auto-create a new TTY window on OS/2 (currently one
#      extra window per session - need named pipes to have more...);
#   d) Simplified interface for custom createTTY functions (with a backward
#      compatibility hack); now returns the TTY name to use; return of ''
#      means that the function reset the I/O handles itself;
#   d') Better message on the semantic of custom createTTY function;
#   e) Convert the existing code to create a TTY into a custom createTTY
#      function;
#   f) Consistent support for TTY names of the form "TTYin,TTYout";
#   g) Switch line-tracing output too to the created TTY window;
#   h) make `b fork' DWIM with CORE::GLOBAL::fork;
#   i) High-level debugger API cmd_*():
#      cmd_b_load($filenamepart)            # b load filenamepart
#      cmd_b_line($lineno [, $cond])        # b lineno [cond]
#      cmd_b_sub($sub [, $cond])            # b sub [cond]
#      cmd_stop()                           # Control-C
#      cmd_d($lineno)                       # d lineno (B)
#      The cmd_*() API returns FALSE on failure; in this case it outputs
#      the error message to the debugging output.
#   j) Low-level debugger API
#      break_on_load($filename)             # b load filename
#      @files = report_break_on_load()      # List files with load-breakpoints
#      breakable_line_in_filename($name, $from [, $to])
#                                           # First breakable line in the
#                                           # range $from .. $to.  $to defaults
#                                           # to $from, and may be less than $to
#      breakable_line($from [, $to])        # Same for the current file
#      break_on_filename_line($name, $lineno [, $cond])
#                                           # Set breakpoint,$cond defaults to 1
#      break_on_filename_line_range($name, $from, $to [, $cond])
#                                           # As above, on the first
#                                           # breakable line in range
#      break_on_line($lineno [, $cond])     # As above, in the current file
#      break_subroutine($sub [, $cond])     # break on the first breakable line
#      ($name, $from, $to) = subroutine_filename_lines($sub)
#                                           # The range of lines of the text
#      The low-level API returns TRUE on success, and die()s on failure.
#
# Changes: 1.10: May 23, 2001  Daniel Lewart <d-lewart@uiuc.edu>
#   BUG FIXES:
#   + Fixed warnings generated by "perl -dWe 42"
#   + Corrected spelling errors
#   + Squeezed Help (h) output into 80 columns
#
# Changes: 1.11: May 24, 2001  David Dyck <dcd@tc.fluke.com>
#   + Made "x @INC" work like it used to
#
# Changes: 1.12: May 24, 2001  Daniel Lewart <d-lewart@uiuc.edu>
#   + Fixed warnings generated by "O" (Show debugger options)
#   + Fixed warnings generated by "p 42" (Print expression)
# Changes: 1.13: Jun 19, 2001 Scott.L.Miller@compaq.com
#   + Added windowSize option 
# Changes: 1.14: Oct  9, 2001 multiple
#   + Clean up after itself on VMS (Charles Lane in 12385)
#   + Adding "@ file" syntax (Peter Scott in 12014)
#   + Debug reloading selfloaded stuff (Ilya Zakharevich in 11457)
#   + $^S and other debugger fixes (Ilya Zakharevich in 11120)
#   + Forgot a my() declaration (Ilya Zakharevich in 11085)
# Changes: 1.15: Nov  6, 2001 Michael G Schwern <schwern@pobox.com>
#   + Updated 1.14 change log
#   + Added *dbline explainatory comments
#   + Mentioning perldebguts man page
# Changes: 1.16: Feb 15, 2002 Mark-Jason Dominus <mjd@plover.com>
#	+ $onetimeDump improvements
# Changes: 1.17: Feb 20, 2002 Richard Foley <richard.foley@rfi.net>
#   Moved some code to cmd_[.]()'s for clarity and ease of handling,
#   rationalised the following commands and added cmd_wrapper() to 
#   enable switching between old and frighteningly consistent new 
#   behaviours for diehards: 'o CommandSet=pre580' (sigh...)
#     a(add),       A(del)            # action expr   (added del by line)
#   + b(add),       B(del)            # break  [line] (was b,D)
#   + w(add),       W(del)            # watch  expr   (was W,W) added del by expr
#   + h(summary), h h(long)           # help (hh)     (was h h,h)
#   + m(methods),   M(modules)        # ...           (was m,v)
#   + o(option)                       # lc            (was O)
#   + v(view code), V(view Variables) # ...           (was w,V)
# Changes: 1.18: Mar 17, 2002 Richard Foley <richard.foley@rfi.net>
#   + fixed missing cmd_O bug
# Changes: 1.19: Mar 29, 2002 Spider Boardman
#   + Added missing local()s -- DB::DB is called recursively.
# 
####################################################################

# Needed for the statement after exec():

BEGIN { $ini_warn = $^W; $^W = 0 } # Switch compilation warnings off until another BEGIN.
local($^W) = 0;			# Switch run-time warnings off during init.
warn (			# Do not ;-)
      $dumpvar::hashDepth,     
      $dumpvar::arrayDepth,    
      $dumpvar::dumpDBFiles,   
      $dumpvar::dumpPackages,  
      $dumpvar::quoteHighBit,  
      $dumpvar::printUndef,    
      $dumpvar::globPrint,     
      $dumpvar::usageOnly,
      @ARGS,
      $Carp::CarpLevel,
      $panic,
      $second_time,
     ) if 0;

# Command-line + PERLLIB:
@ini_INC = @INC;

# $prevwarn = $prevdie = $prevbus = $prevsegv = ''; # Does not help?!

$trace = $signal = $single = 0;	# Uninitialized warning suppression
                                # (local $^W cannot help - other packages!).
$inhibit_exit = $option{PrintRet} = 1;

@options     = qw(hashDepth arrayDepth CommandSet dumpDepth
                  DumpDBFiles DumpPackages DumpReused
		  compactDump veryCompact quote HighBit undefPrint
		  globPrint PrintRet UsageOnly frame AutoTrace
		  TTY noTTY ReadLine NonStop LineInfo maxTraceLen
		  recallCommand ShellBang pager tkRunning ornaments
		  signalLevel warnLevel dieLevel inhibit_exit
		  ImmediateStop bareStringify CreateTTY
		  RemotePort windowSize);

%optionVars    = (
		 hashDepth	=> \$dumpvar::hashDepth,
		 arrayDepth	=> \$dumpvar::arrayDepth,
		 CommandSet => \$CommandSet,
		 DumpDBFiles	=> \$dumpvar::dumpDBFiles,
		 DumpPackages	=> \$dumpvar::dumpPackages,
		 DumpReused	=> \$dumpvar::dumpReused,
		 HighBit	=> \$dumpvar::quoteHighBit,
		 undefPrint	=> \$dumpvar::printUndef,
		 globPrint	=> \$dumpvar::globPrint,
		 UsageOnly	=> \$dumpvar::usageOnly,
		 CreateTTY	=> \$CreateTTY,
		 bareStringify	=> \$dumpvar::bareStringify,
		 frame          => \$frame,
		 AutoTrace      => \$trace,
		 inhibit_exit   => \$inhibit_exit,
		 maxTraceLen	=> \$maxtrace,
		 ImmediateStop	=> \$ImmediateStop,
		 RemotePort	=> \$remoteport,
		 windowSize	=> \$window,
);

%optionAction  = (
		  compactDump	=> \&dumpvar::compactDump,
		  veryCompact	=> \&dumpvar::veryCompact,
		  quote		=> \&dumpvar::quote,
		  TTY		=> \&TTY,
		  noTTY		=> \&noTTY,
		  ReadLine	=> \&ReadLine,
		  NonStop	=> \&NonStop,
		  LineInfo	=> \&LineInfo,
		  recallCommand	=> \&recallCommand,
		  ShellBang	=> \&shellBang,
		  pager		=> \&pager,
		  signalLevel	=> \&signalLevel,
		  warnLevel	=> \&warnLevel,
		  dieLevel	=> \&dieLevel,
		  tkRunning	=> \&tkRunning,
		  ornaments	=> \&ornaments,
		  RemotePort	=> \&RemotePort,
		 );

%optionRequire = (
		  compactDump	=> 'dumpvar.pl',
		  veryCompact	=> 'dumpvar.pl',
		  quote		=> 'dumpvar.pl',
		 );

# These guys may be defined in $ENV{PERL5DB} :
$rl		= 1	unless defined $rl;
$warnLevel	= 1	unless defined $warnLevel;
$dieLevel	= 1	unless defined $dieLevel;
$signalLevel	= 1	unless defined $signalLevel;
$pre		= []	unless defined $pre;
$post		= []	unless defined $post;
$pretype	= []	unless defined $pretype;
$CreateTTY	= 3	unless defined $CreateTTY;
$CommandSet = '580'	unless defined $CommandSet;

warnLevel($warnLevel);
dieLevel($dieLevel);
signalLevel($signalLevel);

pager(
      defined $ENV{PAGER}              ? $ENV{PAGER} :
      eval { require Config } && 
        defined $Config::Config{pager} ? $Config::Config{pager}
                                       : 'more'
     ) unless defined $pager;
setman();
&recallCommand("!") unless defined $prc;
&shellBang("!") unless defined $psh;
sethelp();
$maxtrace = 400 unless defined $maxtrace;
$ini_pids = $ENV{PERLDB_PIDS};
if (defined $ENV{PERLDB_PIDS}) {
  $pids = "[$ENV{PERLDB_PIDS}]";
  $ENV{PERLDB_PIDS} .= "->$$";
  $term_pid = -1;
} else {
  $ENV{PERLDB_PIDS} = "$$";
  $pids = "{pid=$$}";
  $term_pid = $$;
}
$pidprompt = '';
*emacs = $slave_editor if $slave_editor;	# May be used in afterinit()...

if (-e "/dev/tty") {  # this is the wrong metric!
  $rcfile=".perldb";
} else {
  $rcfile="perldb.ini";
}

# This isn't really safe, because there's a race
# between checking and opening.  The solution is to
# open and fstat the handle, but then you have to read and
# eval the contents.  But then the silly thing gets
# your lexical scope, which is unfortunately at best.
sub safe_do { 
    my $file = shift;

    # Just exactly what part of the word "CORE::" don't you understand?
    local $SIG{__WARN__};  
    local $SIG{__DIE__};    

    unless (is_safe_file($file)) {
	CORE::warn <<EO_GRIPE;
perldb: Must not source insecure rcfile $file.
        You or the superuser must be the owner, and it must not 
	be writable by anyone but its owner.
EO_GRIPE
	return;
    } 

    do $file;
    CORE::warn("perldb: couldn't parse $file: $@") if $@;
}


# Verifies that owner is either real user or superuser and that no
# one but owner may write to it.  This function is of limited use
# when called on a path instead of upon a handle, because there are
# no guarantees that filename (by dirent) whose file (by ino) is
# eventually accessed is the same as the one tested. 
# Assumes that the file's existence is not in doubt.
sub is_safe_file {
    my $path = shift;
    stat($path) || return;	# mysteriously vaporized
    my($dev,$ino,$mode,$nlink,$uid,$gid) = stat(_);

    return 0 if $uid != 0 && $uid != $<;
    return 0 if $mode & 022;
    return 1;
}

if (-f $rcfile) {
    safe_do("./$rcfile");
} 
elsif (defined $ENV{HOME} && -f "$ENV{HOME}/$rcfile") {
    safe_do("$ENV{HOME}/$rcfile");
}
elsif (defined $ENV{LOGDIR} && -f "$ENV{LOGDIR}/$rcfile") {
    safe_do("$ENV{LOGDIR}/$rcfile");
}

if (defined $ENV{PERLDB_OPTS}) {
  parse_options($ENV{PERLDB_OPTS});
}

if ( not defined &get_fork_TTY and defined $ENV{TERM} and $ENV{TERM} eq 'xterm'
     and defined $ENV{WINDOWID} and defined $ENV{DISPLAY} ) { # _inside_ XTERM?
    *get_fork_TTY = \&xterm_get_fork_TTY;
} elsif ($^O eq 'os2') {
    *get_fork_TTY = \&os2_get_fork_TTY;
}

# Here begin the unreadable code.  It needs fixing.

if (exists $ENV{PERLDB_RESTART}) {
  delete $ENV{PERLDB_RESTART};
  # $restart = 1;
  @hist = get_list('PERLDB_HIST');
  %break_on_load = get_list("PERLDB_ON_LOAD");
  %postponed = get_list("PERLDB_POSTPONE");
  my @had_breakpoints= get_list("PERLDB_VISITED");
  for (0 .. $#had_breakpoints) {
    my %pf = get_list("PERLDB_FILE_$_");
    $postponed_file{$had_breakpoints[$_]} = \%pf if %pf;
  }
  my %opt = get_list("PERLDB_OPT");
  my ($opt,$val);
  while (($opt,$val) = each %opt) {
    $val =~ s/[\\\']/\\$1/g;
    parse_options("$opt'$val'");
  }
  @INC = get_list("PERLDB_INC");
  @ini_INC = @INC;
  $pretype = [get_list("PERLDB_PRETYPE")];
  $pre = [get_list("PERLDB_PRE")];
  $post = [get_list("PERLDB_POST")];
  @typeahead = get_list("PERLDB_TYPEAHEAD", @typeahead);
}

if ($notty) {
  $runnonstop = 1;
} else {
  # Is Perl being run from a slave editor or graphical debugger?
  $slave_editor = ((defined $main::ARGV[0]) and ($main::ARGV[0] eq '-emacs'));
  $rl = 0, shift(@main::ARGV) if $slave_editor;

  #require Term::ReadLine;

  if ($^O eq 'cygwin') {
    # /dev/tty is binary. use stdin for textmode
    undef $console;
  } elsif (-e "/dev/tty") {
    $console = "/dev/tty";
  } elsif ($^O eq 'dos' or -e "con" or $^O eq 'MSWin32') {
    $console = "con";
  } elsif ($^O eq 'MacOS') {
    if ($MacPerl::Version !~ /MPW/) {
      $console = "Dev:Console:Perl Debug"; # Separate window for application
    } else {
      $console = "Dev:Console";
    }
  } else {
    $console = "sys\$command";
  }

  if (($^O eq 'MSWin32') and ($slave_editor or defined $ENV{EMACS})) {
    $console = undef;
  }

  if ($^O eq 'NetWare') {
	$console = undef;
  }

  # Around a bug:
  if (defined $ENV{OS2_SHELL} and ($slave_editor or $ENV{WINDOWID})) { # In OS/2
    $console = undef;
  }

  if ($^O eq 'epoc') {
    $console = undef;
  }

  $console = $tty if defined $tty;

  if (defined $remoteport) {
    require IO::Socket;
    $OUT = new IO::Socket::INET( Timeout  => '10',
                                 PeerAddr => $remoteport,
                                 Proto    => 'tcp',
                               );
    if (!$OUT) { die "Unable to connect to remote host: $remoteport\n"; }
    $IN = $OUT;
  } else {
    create_IN_OUT(4) if $CreateTTY & 4;
    if ($console) {
      my ($i, $o) = split /,/, $console;
      $o = $i unless defined $o;
      open(IN,"+<$i") || open(IN,"<$i") || open(IN,"<&STDIN");
      open(OUT,"+>$o") || open(OUT,">$o") || open(OUT,">&STDERR")
        || open(OUT,">&STDOUT");	# so we don't dongle stdout
    } elsif (not defined $console) {
      open(IN,"<&STDIN");
      open(OUT,">&STDERR") || open(OUT,">&STDOUT"); # so we don't dongle stdout
      $console = 'STDIN/OUT';
    }
    # so open("|more") can read from STDOUT and so we don't dingle stdin
    $IN = \*IN, $OUT = \*OUT if $console or not defined $console;
  }
  my $previous = select($OUT);
  $| = 1;			# for DB::OUT
  select($previous);

  $LINEINFO = $OUT unless defined $LINEINFO;
  $lineinfo = $console unless defined $lineinfo;

  $header =~ s/.Header: ([^,]+),v(\s+\S+\s+\S+).*$/$1$2/;
  unless ($runnonstop) {
    local $\ = '';
    local $, = '';
    if ($term_pid eq '-1') {
      print $OUT "\nDaughter DB session started...\n";
    } else {
      print $OUT "\nLoading DB routines from $header\n";
      print $OUT ("Editor support ",
		  $slave_editor ? "enabled" : "available",
		  ".\n");
      print $OUT "\nEnter h or `h h' for help, or `$doccmd perldebug' for more help.\n\n";
    }
  }
}

@ARGS = @ARGV;
for (@args) {
    s/\'/\\\'/g;
    s/(.*)/'$1'/ unless /^-?[\d.]+$/;
}

if (defined &afterinit) {	# May be defined in $rcfile
  &afterinit();
}

$I_m_init = 1;

############################################################ Subroutines

sub DB {
    # _After_ the perl program is compiled, $single is set to 1:
    if ($single and not $second_time++) {
      if ($runnonstop) {	# Disable until signal
	for ($i=0; $i <= $stack_depth; ) {
	    $stack[$i++] &= ~1;
	}
	$single = 0;
	# return;			# Would not print trace!
      } elsif ($ImmediateStop) {
	$ImmediateStop = 0;
	$signal = 1;
      }
    }
    $runnonstop = 0 if $single or $signal; # Disable it if interactive.
    &save;
    local($package, $filename, $line) = caller;
    local $filename_ini = $filename;
    local $usercontext = '($@, $!, $^E, $,, $/, $\, $^W) = @saved;' .
      "package $package;";	# this won't let them modify, alas
    local(*dbline) = $main::{'_<' . $filename};

    # we need to check for pseudofiles on Mac OS (these are files
    # not attached to a filename, but instead stored in Dev:Pseudo)
    if ($^O eq 'MacOS' && $#dbline < 0) {
	$filename_ini = $filename = 'Dev:Pseudo';
	*dbline = $main::{'_<' . $filename};
    }

    local $max = $#dbline;
    if ($dbline{$line} && (($stop,$action) = split(/\0/,$dbline{$line}))) {
		if ($stop eq '1') {
			$signal |= 1;
		} elsif ($stop) {
			$evalarg = "\$DB::signal |= 1 if do {$stop}"; &eval;
			$dbline{$line} =~ s/;9($|\0)/$1/;
		}
    }
    my $was_signal = $signal;
    if ($trace & 2) {
      for (my $n = 0; $n <= $#to_watch; $n++) {
		$evalarg = $to_watch[$n];
		local $onetimeDump;	# Do not output results
		my ($val) = &eval;	# Fix context (&eval is doing array)?
		$val = ( (defined $val) ? "'$val'" : 'undef' );
		if ($val ne $old_watch[$n]) {
		  $signal = 1;
		  print $OUT <<EOP;
Watchpoint $n:\t$to_watch[$n] changed:
	old value:\t$old_watch[$n]
	new value:\t$val
EOP
		  $old_watch[$n] = $val;
		}
      }
    }
    if ($trace & 4) {		# User-installed watch
      return if watchfunction($package, $filename, $line) 
	and not $single and not $was_signal and not ($trace & ~4);
    }
    $was_signal = $signal;
    $signal = 0;
    if ($single || ($trace & 1) || $was_signal) {
	if ($slave_editor) {
	    $position = "\032\032$filename:$line:0\n";
	    print_lineinfo($position);
	} elsif ($package eq 'DB::fake') {
	  $term || &setterm;
	  print_help(<<EOP);
Debugged program terminated.  Use B<q> to quit or B<R> to restart,
  use B<O> I<inhibit_exit> to avoid stopping after program termination,
  B<h q>, B<h R> or B<h O> to get additional info.  
EOP
	  $package = 'main';
	  $usercontext = '($@, $!, $^E, $,, $/, $\, $^W) = @saved;' .
	    "package $package;";	# this won't let them modify, alas
	} else {
	    $sub =~ s/\'/::/;
	    $prefix = $sub =~ /::/ ? "" : "${'package'}::";
	    $prefix .= "$sub($filename:";
	    $after = ($dbline[$line] =~ /\n$/ ? '' : "\n");
	    if (length($prefix) > 30) {
	        $position = "$prefix$line):\n$line:\t$dbline[$line]$after";
			$prefix = "";
			$infix = ":\t";
	    } else {
			$infix = "):\t";
			$position = "$prefix$line$infix$dbline[$line]$after";
	    }
	    if ($frame) {
			print_lineinfo(' ' x $stack_depth, "$line:\t$dbline[$line]$after");
	    } else {
			print_lineinfo($position);
	    }
	    for ($i = $line + 1; $i <= $max && $dbline[$i] == 0; ++$i) { #{ vi
			last if $dbline[$i] =~ /^\s*[\;\}\#\n]/;
			last if $signal;
			$after = ($dbline[$i] =~ /\n$/ ? '' : "\n");
			$incr_pos = "$prefix$i$infix$dbline[$i]$after";
			$position .= $incr_pos;
			if ($frame) {
				print_lineinfo(' ' x $stack_depth, "$i:\t$dbline[$i]$after");
			} else {
				print_lineinfo($incr_pos);
			}
	    }
	}
    }
    $evalarg = $action, &eval if $action;
    if ($single || $was_signal) {
	  local $level = $level + 1;
	  foreach $evalarg (@$pre) {
	    &eval;
	  }
	  print $OUT $stack_depth . " levels deep in subroutine calls!\n"
              if $single & 4;
		$start = $line;
		$incr = -1;		# for backward motion.
		@typeahead = (@$pretype, @typeahead);
    CMD:
	while (($term || &setterm),
	       ($term_pid == $$ or resetterm(1)),
	       defined ($cmd=&readline("$pidprompt  DB" . ('<' x $level) .
				       ($#hist+1) . ('>' x $level) . " "))) 
        {
		$single = 0;
		$signal = 0;
		$cmd =~ s/\\$/\n/ && do {
		    $cmd .= &readline("  cont: ");
		    redo CMD;
		};
		$cmd =~ /^$/ && ($cmd = $laststep);
		push(@hist,$cmd) if length($cmd) > 1;
	      PIPE: {
		    $cmd =~ s/^\s+//s;   # trim annoying leading whitespace
		    $cmd =~ s/\s+$//s;   # trim annoying trailing whitespace
		    ($i) = split(/\s+/,$cmd);
		    if ($alias{$i}) { 
					# squelch the sigmangler
					local $SIG{__DIE__};
					local $SIG{__WARN__};
					eval "\$cmd =~ $alias{$i}";
					if ($@) {
                                                local $\ = '';
						print $OUT "Couldn't evaluate `$i' alias: $@";
						next CMD;
					} 
		    }
                    $cmd =~ /^q$/ && do {
                        $fall_off_end = 1;
                        clean_ENV();
                        exit $?;
                    };
		    $cmd =~ /^t$/ && do {
			$trace ^= 1;
			local $\ = '';
			print $OUT "Trace = " .
			    (($trace & 1) ? "on" : "off" ) . "\n";
			next CMD; };
		    $cmd =~ /^S(\s+(!)?(.+))?$/ && do {
			$Srev = defined $2; $Spatt = $3; $Snocheck = ! defined $1;
			local $\ = '';
			local $, = '';
			foreach $subname (sort(keys %sub)) {
			    if ($Snocheck or $Srev^($subname =~ /$Spatt/)) {
				print $OUT $subname,"\n";
			    }
			}
			next CMD; };
		    $cmd =~ s/^X\b/V $package/;
		    $cmd =~ /^V$/ && do {
			$cmd = "V $package"; };
		    $cmd =~ /^V\b\s*(\S+)\s*(.*)/ && do {
			local ($savout) = select($OUT);
			$packname = $1;
			@vars = split(' ',$2);
			do 'dumpvar.pl' unless defined &main::dumpvar;
			if (defined &main::dumpvar) {
			    local $frame = 0;
			    local $doret = -2;
			    # must detect sigpipe failures
                           eval { &main::dumpvar($packname,
                                                 defined $option{dumpDepth}
                                                  ? $option{dumpDepth} : -1,
                                                 @vars) };
			    if ($@) {
				die unless $@ =~ /dumpvar print failed/;
			    } 
			} else {
			    print $OUT "dumpvar.pl not available.\n";
			}
			select ($savout);
			next CMD; };
		    $cmd =~ s/^x\b/ / && do { # So that will be evaled
			$onetimeDump = 'dump'; 
                        # handle special  "x 3 blah" syntax
                        if ($cmd =~ s/^\s*(\d+)(?=\s)/ /) {
                          $onetimedumpDepth = $1;
                        }
                      };
		    $cmd =~ s/^m\s+([\w:]+)\s*$/ / && do {
			methods($1); next CMD};
		    $cmd =~ s/^m\b/ / && do { # So this will be evaled
			$onetimeDump = 'methods'; };
		    $cmd =~ /^f\b\s*(.*)/ && do {
			$file = $1;
			$file =~ s/\s+$//;
			if (!$file) {
			    print $OUT "The old f command is now the r command.\n"; # hint
			    print $OUT "The new f command switches filenames.\n";
			    next CMD;
			}
			if (!defined $main::{'_<' . $file}) {
			    if (($try) = grep(m#^_<.*$file#, keys %main::)) {{
					      $try = substr($try,2);
					      print $OUT "Choosing $try matching `$file':\n";
					      $file = $try;
					  }}
			}
			if (!defined $main::{'_<' . $file}) {
			    print $OUT "No file matching `$file' is loaded.\n";
			    next CMD;
			} elsif ($file ne $filename) {
			    *dbline = $main::{'_<' . $file};
			    $max = $#dbline;
			    $filename = $file;
			    $start = 1;
			    $cmd = "l";
			  } else {
			    print $OUT "Already in $file.\n";
			    next CMD;
			  }
		      };
		    $cmd =~ /^\.$/ && do {
			$incr = -1;		# for backward motion.
			$start = $line;
			$filename = $filename_ini;
			*dbline = $main::{'_<' . $filename};
			$max = $#dbline;
			print_lineinfo($position);
			next CMD };
		    $cmd =~ /^-$/ && do {
			$start -= $incr + $window + 1;
			$start = 1 if $start <= 0;
			$incr = $window - 1;
			$cmd = 'l ' . ($start) . '+'; };
			# rjsf ->
		  $cmd =~ /^([aAbBhlLMoOvwW])\b\s*(.*)/s && do { 
				&cmd_wrapper($1, $2, $line); 
				next CMD; 
			};
			# <- rjsf
		  $cmd =~ /^\<\<\s*(.*)/ && do { # \<\< for CPerl sake: not HERE
			push @$pre, action($1);
			next CMD; };
		    $cmd =~ /^>>\s*(.*)/ && do {
			push @$post, action($1);
			next CMD; };
		    $cmd =~ /^<\s*(.*)/ && do {
			unless ($1) {
			    print $OUT "All < actions cleared.\n";
			    $pre = [];
			    next CMD;
			} 
			if ($1 eq '?') {
			    unless (@$pre) {
				print $OUT "No pre-prompt Perl actions.\n";
				next CMD;
			    } 
			    print $OUT "Perl commands run before each prompt:\n";
			    for my $action ( @$pre ) {
				print $OUT "\t< -- $action\n";
			    } 
			    next CMD;
			} 
			$pre = [action($1)];
			next CMD; };
		    $cmd =~ /^>\s*(.*)/ && do {
			unless ($1) {
			    print $OUT "All > actions cleared.\n";
			    $post = [];
			    next CMD;
			}
			if ($1 eq '?') {
			    unless (@$post) {
				print $OUT "No post-prompt Perl actions.\n";
				next CMD;
			    } 
			    print $OUT "Perl commands run after each prompt:\n";
			    for my $action ( @$post ) {
				print $OUT "\t> -- $action\n";
			    } 
			    next CMD;
			} 
			$post = [action($1)];
			next CMD; };
		    $cmd =~ /^\{\{\s*(.*)/ && do {
			if ($cmd =~ /^\{.*\}$/ && unbalanced(substr($cmd,2))) { 
			    print $OUT "{{ is now a debugger command\n",
				"use `;{{' if you mean Perl code\n";
			    $cmd = "h {{";
			    redo CMD;
			} 
			push @$pretype, $1;
			next CMD; };
		    $cmd =~ /^\{\s*(.*)/ && do {
			unless ($1) {
			    print $OUT "All { actions cleared.\n";
			    $pretype = [];
			    next CMD;
			}
			if ($1 eq '?') {
			    unless (@$pretype) {
				print $OUT "No pre-prompt debugger actions.\n";
				next CMD;
			    } 
			    print $OUT "Debugger commands run before each prompt:\n";
			    for my $action ( @$pretype ) {
				print $OUT "\t{ -- $action\n";
			    } 
			    next CMD;
			} 
			if ($cmd =~ /^\{.*\}$/ && unbalanced(substr($cmd,1))) { 
			    print $OUT "{ is now a debugger command\n",
				"use `;{' if you mean Perl code\n";
			    $cmd = "h {";
			    redo CMD;
			} 
			$pretype = [$1];
			next CMD; };
                   $cmd =~ /^y(?:\s+(\d*)\s*(.*))?$/ && do {
                       eval { require PadWalker; PadWalker->VERSION(0.08) }
                         or &warn($@ =~ /locate/
                            ? "PadWalker module not found - please install\n"
                            : $@)
                          and next CMD;
                       do 'dumpvar.pl' unless defined &main::dumpvar;
                       defined &main::dumpvar
                          or print $OUT "dumpvar.pl not available.\n"
                          and next CMD;
                       my @vars = split(' ', $2 || '');
                       my $h = eval { PadWalker::peek_my(($1 || 0) + 1) };
                       $@ and $@ =~ s/ at .*//, &warn($@), next CMD;
                       my $savout = select($OUT);
                       dumpvar::dumplex($_, $h->{$_}, 
                                       defined $option{dumpDepth}
                                       ? $option{dumpDepth} : -1,
                                       @vars)
                           for sort keys %$h;
                       select($savout);
                       next CMD; };
                   $cmd =~ /^n$/ && do {
		        end_report(), next CMD if $finished and $level <= 1;
			$single = 2;
			$laststep = $cmd;
			last CMD; };
		    $cmd =~ /^s$/ && do {
		        end_report(), next CMD if $finished and $level <= 1;
			$single = 1;
			$laststep = $cmd;
			last CMD; };
		    $cmd =~ /^c\b\s*([\w:]*)\s*$/ && do {
		        end_report(), next CMD if $finished and $level <= 1;
			$subname = $i = $1;
			#  Probably not needed, since we finish an interactive
			#  sub-session anyway...
			# local $filename = $filename;
			# local *dbline = *dbline;	# XXX Would this work?!
			if ($subname =~ /\D/) { # subroutine name
			    $subname = $package."::".$subname 
			        unless $subname =~ /::/;
			    ($file,$i) = (find_sub($subname) =~ /^(.*):(.*)$/);
			    $i += 0;
			    if ($i) {
			        $filename = $file;
				*dbline = $main::{'_<' . $filename};
				$had_breakpoints{$filename} |= 1;
				$max = $#dbline;
				++$i while $dbline[$i] == 0 && $i < $max;
			    } else {
				print $OUT "Subroutine $subname not found.\n";
				next CMD; 
			    }
			}
			if ($i) {
			    if ($dbline[$i] == 0) {
				print $OUT "Line $i not breakable.\n";
				next CMD;
			    }
			    $dbline{$i} =~ s/($|\0)/;9$1/; # add one-time-only b.p.
			}
			for ($i=0; $i <= $stack_depth; ) {
			    $stack[$i++] &= ~1;
			}
			last CMD; };
		    $cmd =~ /^r$/ && do {
		        end_report(), next CMD if $finished and $level <= 1;
			$stack[$stack_depth] |= 1;
			$doret = $option{PrintRet} ? $stack_depth - 1 : -2;
			last CMD; };
		    $cmd =~ /^R$/ && do {
		        print $OUT "Warning: some settings and command-line options may be lost!\n";
			my (@script, @flags, $cl);
			push @flags, '-w' if $ini_warn;
			# Put all the old includes at the start to get
			# the same debugger.
			for (@ini_INC) {
			  push @flags, '-I', $_;
			}
			push @flags, '-T' if ${^TAINT};
			# Arrange for setting the old INC:
			set_list("PERLDB_INC", @ini_INC);
			if ($0 eq '-e') {
			  for (1..$#{'::_<-e'}) { # The first line is PERL5DB
			        chomp ($cl =  ${'::_<-e'}[$_]);
			    push @script, '-e', $cl;
			  }
			} else {
			  @script = $0;
			}
			set_list("PERLDB_HIST", 
				 $term->Features->{getHistory} 
				 ? $term->GetHistory : @hist);
			my @had_breakpoints = keys %had_breakpoints;
			set_list("PERLDB_VISITED", @had_breakpoints);
			set_list("PERLDB_OPT", %option);
			set_list("PERLDB_ON_LOAD", %break_on_load);
			my @hard;
			for (0 .. $#had_breakpoints) {
			  my $file = $had_breakpoints[$_];
			  *dbline = $main::{'_<' . $file};
			  next unless %dbline or $postponed_file{$file};
			  (push @hard, $file), next 
			    if $file =~ /^\(\w*eval/;
			  my @add;
			  @add = %{$postponed_file{$file}}
			    if $postponed_file{$file};
			  set_list("PERLDB_FILE_$_", %dbline, @add);
			}
			for (@hard) { # Yes, really-really...
			  # Find the subroutines in this eval
			  *dbline = $main::{'_<' . $_};
			  my ($quoted, $sub, %subs, $line) = quotemeta $_;
			  for $sub (keys %sub) {
			    next unless $sub{$sub} =~ /^$quoted:(\d+)-(\d+)$/;
			    $subs{$sub} = [$1, $2];
			  }
			  unless (%subs) {
			    print $OUT
			      "No subroutines in $_, ignoring breakpoints.\n";
			    next;
			  }
			LINES: for $line (keys %dbline) {
			    # One breakpoint per sub only:
			    my ($offset, $sub, $found);
			  SUBS: for $sub (keys %subs) {
			      if ($subs{$sub}->[1] >= $line # Not after the subroutine
				  and (not defined $offset # Not caught
				       or $offset < 0 )) { # or badly caught
				$found = $sub;
				$offset = $line - $subs{$sub}->[0];
				$offset = "+$offset", last SUBS if $offset >= 0;
			      }
			    }
			    if (defined $offset) {
			      $postponed{$found} =
				"break $offset if $dbline{$line}";
			    } else {
			      print $OUT "Breakpoint in $_:$line ignored: after all the subroutines.\n";
			    }
			  }
			}
			set_list("PERLDB_POSTPONE", %postponed);
			set_list("PERLDB_PRETYPE", @$pretype);
			set_list("PERLDB_PRE", @$pre);
			set_list("PERLDB_POST", @$post);
			set_list("PERLDB_TYPEAHEAD", @typeahead);
			$ENV{PERLDB_RESTART} = 1;
			delete $ENV{PERLDB_PIDS}; # Restore ini state
			$ENV{PERLDB_PIDS} = $ini_pids if defined $ini_pids;
			#print "$^X, '-d', @flags, @script, ($slave_editor ? '-emacs' : ()), @ARGS";
			exec($^X, '-d', @flags, @script, ($slave_editor ? '-emacs' : ()), @ARGS) ||
			print $OUT "exec failed: $!\n";
			last CMD; };
		    $cmd =~ /^T$/ && do {
			print_trace($OUT, 1); # skip DB
			next CMD; };
		    $cmd =~ /^w\b\s*(.*)/s && do { &cmd_w($1); next CMD; };
		    $cmd =~ /^W\b\s*(.*)/s && do { &cmd_W($1); next CMD; };
		    $cmd =~ /^\/(.*)$/ && do {
			$inpat = $1;
			$inpat =~ s:([^\\])/$:$1:;
			if ($inpat ne "") {
			    # squelch the sigmangler
			    local $SIG{__DIE__};
			    local $SIG{__WARN__};
			    eval '$inpat =~ m'."\a$inpat\a";	
			    if ($@ ne "") {
				print $OUT "$@";
				next CMD;
			    }
			    $pat = $inpat;
			}
			$end = $start;
			$incr = -1;
			eval '
			    for (;;) {
				++$start;
				$start = 1 if ($start > $max);
				last if ($start == $end);
				if ($dbline[$start] =~ m' . "\a$pat\a" . 'i) {
				    if ($slave_editor) {
					print $OUT "\032\032$filename:$start:0\n";
				    } else {
					print $OUT "$start:\t", $dbline[$start], "\n";
				    }
				    last;
				}
			    } ';
			print $OUT "/$pat/: not found\n" if ($start == $end);
			next CMD; };
		    $cmd =~ /^\?(.*)$/ && do {
			$inpat = $1;
			$inpat =~ s:([^\\])\?$:$1:;
			if ($inpat ne "") {
			    # squelch the sigmangler
			    local $SIG{__DIE__};
			    local $SIG{__WARN__};
			    eval '$inpat =~ m'."\a$inpat\a";	
			    if ($@ ne "") {
				print $OUT $@;
				next CMD;
			    }
			    $pat = $inpat;
			}
			$end = $start;
			$incr = -1;
			eval '
			    for (;;) {
				--$start;
				$start = $max if ($start <= 0);
				last if ($start == $end);
				if ($dbline[$start] =~ m' . "\a$pat\a" . 'i) {
				    if ($slave_editor) {
					print $OUT "\032\032$filename:$start:0\n";
				    } else {
					print $OUT "$start:\t", $dbline[$start], "\n";
				    }
				    last;
				}
			    } ';
			print $OUT "?$pat?: not found\n" if ($start == $end);
			next CMD; };
		    $cmd =~ /^$rc+\s*(-)?(\d+)?$/ && do {
			pop(@hist) if length($cmd) > 1;
			$i = $1 ? ($#hist-($2||1)) : ($2||$#hist);
			$cmd = $hist[$i];
			print $OUT $cmd, "\n";
			redo CMD; };
		    $cmd =~ /^$sh$sh\s*([\x00-\xff]*)/ && do {
			&system($1);
			next CMD; };
		    $cmd =~ /^$rc([^$rc].*)$/ && do {
			$pat = "^$1";
			pop(@hist) if length($cmd) > 1;
			for ($i = $#hist; $i; --$i) {
			    last if $hist[$i] =~ /$pat/;
			}
			if (!$i) {
			    print $OUT "No such command!\n\n";
			    next CMD;
			}
			$cmd = $hist[$i];
			print $OUT $cmd, "\n";
			redo CMD; };
		    $cmd =~ /^$sh$/ && do {
			&system($ENV{SHELL}||"/bin/sh");
			next CMD; };
		    $cmd =~ /^$sh\s*([\x00-\xff]*)/ && do {
			# XXX: using csh or tcsh destroys sigint retvals!
			#&system($1);  # use this instead
			&system($ENV{SHELL}||"/bin/sh","-c",$1);
			next CMD; };
		    $cmd =~ /^H\b\s*(-(\d+))?/ && do {
			$end = $2 ? ($#hist-$2) : 0;
			$hist = 0 if $hist < 0;
			for ($i=$#hist; $i>$end; $i--) {
			    print $OUT "$i: ",$hist[$i],"\n"
			      unless $hist[$i] =~ /^.?$/;
			};
			next CMD; };
		    $cmd =~ /^(?:man|(?:perl)?doc)\b(?:\s+([^(]*))?$/ && do {
			runman($1);
			next CMD; };
		    $cmd =~ s/^p$/print {\$DB::OUT} \$_/;
		    $cmd =~ s/^p\b/print {\$DB::OUT} /;
		    $cmd =~ s/^=\s*// && do {
			my @keys;
			if (length $cmd == 0) {
			    @keys = sort keys %alias;
			} elsif (my($k,$v) = ($cmd =~ /^(\S+)\s+(\S.*)/)) {
			    # can't use $_ or kill //g state
			    for my $x ($k, $v) { $x =~ s/\a/\\a/g }
			    $alias{$k} = "s\a$k\a$v\a";
			    # squelch the sigmangler
			    local $SIG{__DIE__};
			    local $SIG{__WARN__};
			    unless (eval "sub { s\a$k\a$v\a }; 1") {
				print $OUT "Can't alias $k to $v: $@\n"; 
				delete $alias{$k};
				next CMD;
			    } 
			    @keys = ($k);
			} else {
			    @keys = ($cmd);
			} 
			for my $k (@keys) {
			    if ((my $v = $alias{$k}) =~ ss\a$k\a(.*)\a$1) {
				print $OUT "$k\t= $1\n";
			    } 
			    elsif (defined $alias{$k}) {
				    print $OUT "$k\t$alias{$k}\n";
			    } 
			    else {
				print "No alias for $k\n";
			    } 
			}
			next CMD; };
                    $cmd =~ /^source\s+(.*\S)/ && do {
		      if (open my $fh, $1) {
			push @cmdfhs, $fh;
		      } else {
			&warn("Can't execute `$1': $!\n");
		      }
		      next CMD; };
		    $cmd =~ /^\|\|?\s*[^|]/ && do {
			if ($pager =~ /^\|/) {
			    open(SAVEOUT,">&STDOUT") || &warn("Can't save STDOUT");
			    open(STDOUT,">&OUT") || &warn("Can't redirect STDOUT");
			} else {
			    open(SAVEOUT,">&OUT") || &warn("Can't save DB::OUT");
			}
			fix_less();
			unless ($piped=open(OUT,$pager)) {
			    &warn("Can't pipe output to `$pager'");
			    if ($pager =~ /^\|/) {
				open(OUT,">&STDOUT") # XXX: lost message
				    || &warn("Can't restore DB::OUT");
				open(STDOUT,">&SAVEOUT")
				  || &warn("Can't restore STDOUT");
				close(SAVEOUT);
			    } else {
				open(OUT,">&STDOUT") # XXX: lost message
				    || &warn("Can't restore DB::OUT");
			    }
			    next CMD;
			}
			$SIG{PIPE}= \&DB::catch if $pager =~ /^\|/
			    && ("" eq $SIG{PIPE}  ||  "DEFAULT" eq $SIG{PIPE});
			$selected= select(OUT);
			$|= 1;
			select( $selected ), $selected= "" unless $cmd =~ /^\|\|/;
			$cmd =~ s/^\|+\s*//;
			redo PIPE; 
		    };
		    # XXX Local variants do not work!
		    $cmd =~ s/^t\s/\$DB::trace |= 1;\n/;
		    $cmd =~ s/^s\s/\$DB::single = 1;\n/ && do {$laststep = 's'};
		    $cmd =~ s/^n\s/\$DB::single = 2;\n/ && do {$laststep = 'n'};
		}		# PIPE:
	    $evalarg = "\$^D = \$^D | \$DB::db_stop;\n$cmd"; &eval;
	    if ($onetimeDump) {
		$onetimeDump = undef;
                $onetimedumpDepth = undef;
	    } elsif ($term_pid == $$) {
		print $OUT "\n";
	    }
	} continue {		# CMD:
	    if ($piped) {
		if ($pager =~ /^\|/) {
		    $? = 0;  
		    # we cannot warn here: the handle is missing --tchrist
		    close(OUT) || print SAVEOUT "\nCan't close DB::OUT\n";

		    # most of the $? crud was coping with broken cshisms
		    if ($?) {
			print SAVEOUT "Pager `$pager' failed: ";
			if ($? == -1) {
			    print SAVEOUT "shell returned -1\n";
			} elsif ($? >> 8) {
			    print SAVEOUT 
			      ( $? & 127 ) ? " (SIG#".($?&127).")" : "", 
			      ( $? & 128 ) ? " -- core dumped" : "", "\n";
			} else {
			    print SAVEOUT "status ", ($? >> 8), "\n";
			} 
		    } 

		    open(OUT,">&STDOUT") || &warn("Can't restore DB::OUT");
		    open(STDOUT,">&SAVEOUT") || &warn("Can't restore STDOUT");
		    $SIG{PIPE} = "DEFAULT" if $SIG{PIPE} eq \&DB::catch;
		    # Will stop ignoring SIGPIPE if done like nohup(1)
		    # does SIGINT but Perl doesn't give us a choice.
		} else {
		    open(OUT,">&SAVEOUT") || &warn("Can't restore DB::OUT");
		}
		close(SAVEOUT);
		select($selected), $selected= "" unless $selected eq "";
		$piped= "";
	    }
	}			# CMD:
    $fall_off_end = 1 unless defined $cmd; # Emulate `q' on EOF
	foreach $evalarg (@$post) {
	  &eval;
	}
    }				# if ($single || $signal)
    ($@, $!, $^E, $,, $/, $\, $^W) = @saved;
    ();
}

# The following code may be executed now:
# BEGIN {warn 4}

sub sub {
    my ($al, $ret, @ret) = "";
    if (length($sub) > 10 && substr($sub, -10, 10) eq '::AUTOLOAD') {
	$al = " for $$sub";
    }
    local $stack_depth = $stack_depth + 1; # Protect from non-local exits
    $#stack = $stack_depth;
    $stack[-1] = $single;
    $single &= 1;
    $single |= 4 if $stack_depth == $deep;
    ($frame & 4 
     ? ( print_lineinfo(' ' x ($stack_depth - 1), "in  "),
	 # Why -1? But it works! :-(
	 print_trace($LINEINFO, -1, 1, 1, "$sub$al") )
     : print_lineinfo(' ' x ($stack_depth - 1), "entering $sub$al\n")) if $frame;
    if (wantarray) {
	@ret = &$sub;
	$single |= $stack[$stack_depth--];
	($frame & 4 
	 ? ( print_lineinfo(' ' x $stack_depth, "out "), 
	     print_trace($LINEINFO, -1, 1, 1, "$sub$al") )
	 : print_lineinfo(' ' x $stack_depth, "exited $sub$al\n")) if $frame & 2;
	if ($doret eq $stack_depth or $frame & 16) {
	    local $\ = '';
            my $fh = ($doret eq $stack_depth ? $OUT : $LINEINFO);
	    print $fh ' ' x $stack_depth if $frame & 16;
	    print $fh "list context return from $sub:\n"; 
	    dumpit($fh, \@ret );
	    $doret = -2;
	}
	@ret;
    } else {
        if (defined wantarray) {
	    $ret = &$sub;
        } else {
            &$sub; undef $ret;
        };
	$single |= $stack[$stack_depth--];
	($frame & 4 
	 ? (  print_lineinfo(' ' x $stack_depth, "out "),
	      print_trace($LINEINFO, -1, 1, 1, "$sub$al") )
	 : print_lineinfo(' ' x $stack_depth, "exited $sub$al\n")) if $frame & 2;
	if ($doret eq $stack_depth or $frame & 16 and defined wantarray) {
	    local $\ = '';
            my $fh = ($doret eq $stack_depth ? $OUT : $LINEINFO);
	    print $fh (' ' x $stack_depth) if $frame & 16;
	    print $fh (defined wantarray 
			 ? "scalar context return from $sub: " 
			 : "void context return from $sub\n");
	    dumpit( $fh, $ret ) if defined wantarray;
	    $doret = -2;
	}
	$ret;
    }
}

### The API section

### Functions with multiple modes of failure die on error, the rest
### returns FALSE on error.
### User-interface functions cmd_* output error message.

### Note all cmd_[a-zA-Z]'s require $line, $dblineno as first arguments

my %set = ( # 
	'pre580'	=> {
		'a'	=> 'pre580_a', 
		'A'	=> 'pre580_null',
		'b'	=> 'pre580_b', 
		'B'	=> 'pre580_null',
		'd'	=> 'pre580_null',
		'D'	=> 'pre580_D',
		'h'	=> 'pre580_h',
		'M'	=> 'pre580_null',
		'O'	=> 'o',
		'o'	=> 'pre580_null',
		'v'	=> 'M',
		'w'	=> 'v',
		'W'	=> 'pre580_W',
	},
);

sub cmd_wrapper {
	my $cmd      = shift;
	my $line     = shift;
	my $dblineno = shift;

	# with this level of indirection we can wrap 
	# to old (pre580) or other command sets easily
	# 
	my $call = 'cmd_'.(
		$set{$CommandSet}{$cmd} || $cmd
	);
	# print "cmd_wrapper($cmd): $CommandSet($set{$CommandSet}{$cmd}) => call($call)\n";

	return &$call($line, $dblineno);
}

sub cmd_a {
	my $line   = shift || ''; # [.|line] expr
	my $dbline = shift; $line =~ s/^(\.|(?:[^\d]))/$dbline/;
	if ($line =~ /^\s*(\d*)\s*(\S.+)/) {
		my ($lineno, $expr) = ($1, $2);
		if (length $expr) {
			if ($dbline[$lineno] == 0) {
				print $OUT "Line $lineno($dbline[$lineno]) does not have an action?\n";
			} else {
				$had_breakpoints{$filename} |= 2;
				$dbline{$lineno} =~ s/\0[^\0]*//;
				$dbline{$lineno} .= "\0" . action($expr);
			}
		}
	} else {
		print $OUT "Adding an action requires an optional lineno and an expression\n"; # hint
	}
}

sub cmd_A {
	my $line   = shift || '';
	my $dbline = shift; $line =~ s/^\./$dbline/;
	if ($line eq '*') {
		eval { &delete_action(); 1 } or print $OUT $@ and return;
	} elsif ($line =~ /^(\S.*)/) {
		eval { &delete_action($1); 1 } or print $OUT $@ and return;
	} else {
		print $OUT "Deleting an action requires a line number, or '*' for all\n"; # hint
	}
}

sub delete_action {
  my $i = shift;
  if (defined($i)) {
		die "Line $i has no action .\n" if $dbline[$i] == 0;
		$dbline{$i} =~ s/\0[^\0]*//; # \^a
		delete $dbline{$i} if $dbline{$i} eq '';
	} else {
		print $OUT "Deleting all actions...\n";
		for my $file (keys %had_breakpoints) {
			local *dbline = $main::{'_<' . $file};
			my $max = $#dbline;
			my $was;
			for ($i = 1; $i <= $max ; $i++) {
					if (defined $dbline{$i}) {
							$dbline{$i} =~ s/\0[^\0]*//;
							delete $dbline{$i} if $dbline{$i} eq '';
					}
				unless ($had_breakpoints{$file} &= ~2) {
						delete $had_breakpoints{$file};
				}
			}
		}
	}
}

sub cmd_b {
	my $line   = shift; # [.|line] [cond]
	my $dbline = shift; $line =~ s/^\./$dbline/;
	if ($line =~ /^\s*$/) {
		&cmd_b_line($dbline, 1);
	} elsif ($line =~ /^load\b\s*(.*)/) {
		my $file = $1; $file =~ s/\s+$//;
		&cmd_b_load($file);
	} elsif ($line =~ /^(postpone|compile)\b\s*([':A-Za-z_][':\w]*)\s*(.*)/) {
		my $cond = length $3 ? $3 : '1';
		my ($subname, $break) = ($2, $1 eq 'postpone');
		$subname =~ s/\'/::/g;
		$subname = "${'package'}::" . $subname unless $subname =~ /::/;
		$subname = "main".$subname if substr($subname,0,2) eq "::";
		$postponed{$subname} = $break ? "break +0 if $cond" : "compile";
	} elsif ($line =~ /^([':A-Za-z_][':\w]*(?:\[.*\])?)\s*(.*)/) { 
		$subname = $1;
		$cond = length $2 ? $2 : '1';
		&cmd_b_sub($subname, $cond);
	} elsif ($line =~ /^(\d*)\s*(.*)/) { 
		$line = $1 || $dbline;
		$cond = length $2 ? $2 : '1';
		&cmd_b_line($line, $cond);
	} else {
		print "confused by line($line)?\n";
	}
}

sub break_on_load {
  my $file = shift;
  $break_on_load{$file} = 1;
  $had_breakpoints{$file} |= 1;
}

sub report_break_on_load {
  sort keys %break_on_load;
}

sub cmd_b_load {
  my $file = shift;
  my @files;
  {
    push @files, $file;
    push @files, $::INC{$file} if $::INC{$file};
    $file .= '.pm', redo unless $file =~ /\./;
  }
  break_on_load($_) for @files;
  @files = report_break_on_load;
  local $\ = '';
  local $" = ' ';
  print $OUT "Will stop on load of `@files'.\n";
}

$filename_error = '';

sub breakable_line {
  my ($from, $to) = @_;
  my $i = $from;
  if (@_ >= 2) {
    my $delta = $from < $to ? +1 : -1;
    my $limit = $delta > 0 ? $#dbline : 1;
    $limit = $to if ($limit - $to) * $delta > 0;
    $i += $delta while $dbline[$i] == 0 and ($limit - $i) * $delta > 0;
  }
  return $i unless $dbline[$i] == 0;
  my ($pl, $upto) = ('', '');
  ($pl, $upto) = ('s', "..$to") if @_ >=2 and $from != $to;
  die "Line$pl $from$upto$filename_error not breakable\n";
}

sub breakable_line_in_filename {
  my ($f) = shift;
  local *dbline = $main::{'_<' . $f};
  local $filename_error = " of `$f'";
  breakable_line(@_);
}

sub break_on_line {
  my ($i, $cond) = @_;
  $cond = 1 unless @_ >= 2;
  my $inii = $i;
  my $after = '';
  my $pl = '';
  die "Line $i$filename_error not breakable.\n" if $dbline[$i] == 0;
  $had_breakpoints{$filename} |= 1;
  if ($dbline{$i}) { $dbline{$i} =~ s/^[^\0]*/$cond/; }
  else { $dbline{$i} = $cond; }
}

sub cmd_b_line {
  eval { break_on_line(@_); 1 } or do {
    local $\ = '';
    print $OUT $@ and return;
  };
}

sub break_on_filename_line {
  my ($f, $i, $cond) = @_;
  $cond = 1 unless @_ >= 3;
  local *dbline = $main::{'_<' . $f};
  local $filename_error = " of `$f'";
  local $filename = $f;
  break_on_line($i, $cond);
}

sub break_on_filename_line_range {
  my ($f, $from, $to, $cond) = @_;
  my $i = breakable_line_in_filename($f, $from, $to);
  $cond = 1 unless @_ >= 3;
  break_on_filename_line($f,$i,$cond);
}

sub subroutine_filename_lines {
  my ($subname,$cond) = @_;
  # Filename below can contain ':'
  find_sub($subname) =~ /^(.*):(\d+)-(\d+)$/;
}

sub break_subroutine {
  my $subname = shift;
  my ($file,$s,$e) = subroutine_filename_lines($subname) or
    die "Subroutine $subname not found.\n";
  $cond = 1 unless @_ >= 2;
  break_on_filename_line_range($file,$s,$e,@_);
}

sub cmd_b_sub {
  my ($subname,$cond) = @_;
  $cond = 1 unless @_ >= 2;
  unless (ref $subname eq 'CODE') {
    $subname =~ s/\'/::/g;
    my $s = $subname;
    $subname = "${'package'}::" . $subname
      unless $subname =~ /::/;
    $subname = "CORE::GLOBAL::$s"
      if not defined &$subname and $s !~ /::/ and defined &{"CORE::GLOBAL::$s"};
    $subname = "main".$subname if substr($subname,0,2) eq "::";
  }
  eval { break_subroutine($subname,$cond); 1 } or do {
    local $\ = '';
    print $OUT $@ and return;
  }
}

sub cmd_B {
	my $line   = ($_[0] =~ /^\./) ? $dbline : shift || ''; 
	my $dbline = shift; $line =~ s/^\./$dbline/;
	if ($line eq '*') {
		eval { &delete_breakpoint(); 1 } or print $OUT $@ and return;
	} elsif ($line =~ /^(\S.*)/) {
		eval { &delete_breakpoint($line || $dbline); 1 } or do {
                    local $\ = '';
                    print $OUT $@ and return;
                };
	} else {
		print $OUT "Deleting a breakpoint requires a line number, or '*' for all\n"; # hint
	}
}

sub delete_breakpoint {
  my $i = shift;
  if (defined($i)) {
	  die "Line $i not breakable.\n" if $dbline[$i] == 0;
	  $dbline{$i} =~ s/^[^\0]*//;
	  delete $dbline{$i} if $dbline{$i} eq '';
  } else {
		  print $OUT "Deleting all breakpoints...\n";
		  for my $file (keys %had_breakpoints) {
					local *dbline = $main::{'_<' . $file};
					my $max = $#dbline;
					my $was;
					for ($i = 1; $i <= $max ; $i++) {
							if (defined $dbline{$i}) {
						$dbline{$i} =~ s/^[^\0]+//;
						if ($dbline{$i} =~ s/^\0?$//) {
								delete $dbline{$i};
						}
							}
					}
					if (not $had_breakpoints{$file} &= ~1) {
							delete $had_breakpoints{$file};
					}
		  }
		  undef %postponed;
		  undef %postponed_file;
		  undef %break_on_load;
	}
}

sub cmd_stop {			# As on ^C, but not signal-safy.
  $signal = 1;
}

sub cmd_h {
	my $line   = shift || '';
	if ($line  =~ /^h\s*/) {
		print_help($help);
	} elsif ($line =~ /^(\S.*)$/) { 
			# support long commands; otherwise bogus errors
			# happen when you ask for h on <CR> for example
			my $asked = $1;			# for proper errmsg
			my $qasked = quotemeta($asked); # for searching
			# XXX: finds CR but not <CR>
			if ($help =~ /^<?(?:[IB]<)$qasked/m) {
			  while ($help =~ /^(<?(?:[IB]<)$qasked([\s\S]*?)\n)(?!\s)/mg) {
			    print_help($1);
			  }
			} else {
			    print_help("B<$asked> is not a debugger command.\n");
			}
	} else {
			print_help($summary);
	}
}

sub cmd_l {
	my $line = shift;
	$line =~ s/^-\s*$/-/;
	if ($line =~ /^(\$.*)/s) {
		$evalarg = $2;
		my ($s) = &eval;
		print($OUT "Error: $@\n"), next CMD if $@;
		$s = CvGV_name($s);
		print($OUT "Interpreted as: $1 $s\n");
		$line = "$1 $s";
		&cmd_l($s);
	} elsif ($line =~ /^([\':A-Za-z_][\':\w]*(\[.*\])?)/s) { 
		my $s = $subname = $1;
		$subname =~ s/\'/::/;
		$subname = $package."::".$subname 
		unless $subname =~ /::/;
		$subname = "CORE::GLOBAL::$s"
		if not defined &$subname and $s !~ /::/
			 and defined &{"CORE::GLOBAL::$s"};
		$subname = "main".$subname if substr($subname,0,2) eq "::";
		@pieces = split(/:/,find_sub($subname) || $sub{$subname});
		$subrange = pop @pieces;
		$file = join(':', @pieces);
		if ($file ne $filename) {
			print $OUT "Switching to file '$file'.\n"
		unless $slave_editor;
			*dbline = $main::{'_<' . $file};
			$max = $#dbline;
			$filename = $file;
		}
		if ($subrange) {
			if (eval($subrange) < -$window) {
		$subrange =~ s/-.*/+/;
			}
			$line = $subrange;
			&cmd_l($subrange);
		} else {
			print $OUT "Subroutine $subname not found.\n";
		}
	} elsif ($line =~ /^\s*$/) {
		$incr = $window - 1;
		$line = $start . '-' . ($start + $incr); 
		&cmd_l($line);
	} elsif ($line =~ /^(\d*)\+(\d*)$/) { 
		$start = $1 if $1;
		$incr = $2;
		$incr = $window - 1 unless $incr;
		$line = $start . '-' . ($start + $incr); 
		&cmd_l($line);	
	} elsif ($line =~ /^((-?[\d\$\.]+)([-,]([\d\$\.]+))?)?/) { 
		$end = (!defined $2) ? $max : ($4 ? $4 : $2);
		$end = $max if $end > $max;
		$i = $2;
		$i = $line if $i eq '.';
		$i = 1 if $i < 1;
		$incr = $end - $i;
		if ($slave_editor) {
			print $OUT "\032\032$filename:$i:0\n";
			$i = $end;
		} else {
			for (; $i <= $end; $i++) {
				my ($stop,$action);
				($stop,$action) = split(/\0/, $dbline{$i}) if
						$dbline{$i};
							$arrow = ($i==$line 
						and $filename eq $filename_ini) 
					?  '==>' 
						: ($dbline[$i]+0 ? ':' : ' ') ;
				$arrow .= 'b' if $stop;
				$arrow .= 'a' if $action;
				print $OUT "$i$arrow\t", $dbline[$i];
				$i++, last if $signal;
			}
			print $OUT "\n" unless $dbline[$i-1] =~ /\n$/;
		}
		$start = $i; # remember in case they want more
		$start = $max if $start > $max;
	}
}

sub cmd_L {
	my $arg    = shift || 'abw'; $arg = 'abw' unless $CommandSet eq '580'; # sigh...
	my $action_wanted = ($arg =~ /a/) ? 1 : 0;
	my $break_wanted  = ($arg =~ /b/) ? 1 : 0;
	my $watch_wanted  = ($arg =~ /w/) ? 1 : 0;

	if ($break_wanted or $action_wanted) {
		for my $file (keys %had_breakpoints) {
			local *dbline = $main::{'_<' . $file};
			my $max = $#dbline;
			my $was;
			for ($i = 1; $i <= $max; $i++) {
				if (defined $dbline{$i}) {
					print $OUT "$file:\n" unless $was++;
					print $OUT " $i:\t", $dbline[$i];
					($stop,$action) = split(/\0/, $dbline{$i});
					print $OUT "   break if (", $stop, ")\n"
						if $stop and $break_wanted;
					print $OUT "   action:  ", $action, "\n"
						if $action and $action_wanted;
					last if $signal;
				}
			}
		}
	}
	if (%postponed and $break_wanted) {
		print $OUT "Postponed breakpoints in subroutines:\n";
		my $subname;
		for $subname (keys %postponed) {
		  print $OUT " $subname\t$postponed{$subname}\n";
		  last if $signal;
		}
	}
	my @have = map { # Combined keys
			keys %{$postponed_file{$_}}
	} keys %postponed_file;
	if (@have and ($break_wanted or $action_wanted)) {
		print $OUT "Postponed breakpoints in files:\n";
		my ($file, $line);
		for $file (keys %postponed_file) {
		  my $db = $postponed_file{$file};
		  print $OUT " $file:\n";
		  for $line (sort {$a <=> $b} keys %$db) {
			print $OUT "  $line:\n";
			my ($stop,$action) = split(/\0/, $$db{$line});
			print $OUT "    break if (", $stop, ")\n"
			  if $stop and $break_wanted;
			print $OUT "    action:  ", $action, "\n"
			  if $action and $action_wanted;
			last if $signal;
		  }
		  last if $signal;
		}
	}
  if (%break_on_load and $break_wanted) {
		print $OUT "Breakpoints on load:\n";
		my $file;
		for $file (keys %break_on_load) {
		  print $OUT " $file\n";
		  last if $signal;
		}
  }
  if ($watch_wanted) {
	if ($trace & 2) {
		print $OUT "Watch-expressions:\n" if @to_watch;
		for my $expr (@to_watch) {
			print $OUT " $expr\n";
			last if $signal;
		}
	}
  }
}

sub cmd_M {
	&list_modules();
}

sub cmd_o {
	my $opt      = shift || ''; # opt[=val]
	if ($opt =~ /^(\S.*)/) {
		&parse_options($1);
	} else {
		for (@options) {
			&dump_option($_);
		}
	}
}

sub cmd_O {
	print $OUT "The old O command is now the o command.\n";        # hint
	print $OUT "Use 'h' to get current command help synopsis or\n"; # 
	print $OUT "use 'o CommandSet=pre580' to revert to old usage\n"; # 
}

sub cmd_v {
	my $line = shift;

	if ($line =~ /^(\d*)$/) {
		$incr = $window - 1;
		$start = $1 if $1;
		$start -= $preview;
		$line = $start . '-' . ($start + $incr);
		&cmd_l($line);
	}
}

sub cmd_w {
	my $expr     = shift || '';
	if ($expr =~ /^(\S.*)/) {
		push @to_watch, $expr;
		$evalarg = $expr;
		my ($val) = &eval;
		$val = (defined $val) ? "'$val'" : 'undef' ;
		push @old_watch, $val;
		$trace |= 2;
	} else {
		print $OUT "Adding a watch-expression requires an expression\n"; # hint
	}
}

sub cmd_W {
	my $expr     = shift || '';
	if ($expr eq '*') {
		$trace &= ~2;
		print $OUT "Deleting all watch expressions ...\n";
		@to_watch = @old_watch = ();
	} elsif ($expr =~ /^(\S.*)/) {
		my $i_cnt = 0;
		foreach (@to_watch) {
			my $val = $to_watch[$i_cnt];
			if ($val eq $expr) { # =~ m/^\Q$i$/) {
				splice(@to_watch, $i_cnt, 1);
			}
			$i_cnt++;
		}
	} else {
		print $OUT "Deleting a watch-expression requires an expression, or '*' for all\n"; # hint
	}
}

### END of the API section

sub save {
    @saved = ($@, $!, $^E, $,, $/, $\, $^W);
    $, = ""; $/ = "\n"; $\ = ""; $^W = 0;
}

sub print_lineinfo {
  resetterm(1) if $LINEINFO eq $OUT and $term_pid != $$;
  local $\ = '';
  local $, = '';
  print $LINEINFO @_;
}

# The following takes its argument via $evalarg to preserve current @_

sub postponed_sub {
  my $subname = shift;
  if ($postponed{$subname} =~ s/^break\s([+-]?\d+)\s+if\s//) {
    my $offset = $1 || 0;
    # Filename below can contain ':'
    my ($file,$i) = (find_sub($subname) =~ /^(.*):(\d+)-.*$/);
    if ($i) {
      $i += $offset;
      local *dbline = $main::{'_<' . $file};
      local $^W = 0;		# != 0 is magical below
      $had_breakpoints{$file} |= 1;
      my $max = $#dbline;
      ++$i until $dbline[$i] != 0 or $i >= $max;
      $dbline{$i} = delete $postponed{$subname};
    } else {
      local $\ = '';
      print $OUT "Subroutine $subname not found.\n";
    }
    return;
  }
  elsif ($postponed{$subname} eq 'compile') { $signal = 1 }
  #print $OUT "In postponed_sub for `$subname'.\n";
}

sub postponed {
  if ($ImmediateStop) {
    $ImmediateStop = 0;
    $signal = 1;
  }
  return &postponed_sub
    unless ref \$_[0] eq 'GLOB'; # A subroutine is compiled.
  # Cannot be done before the file is compiled
  local *dbline = shift;
  my $filename = $dbline;
  $filename =~ s/^_<//;
  local $\ = '';
  $signal = 1, print $OUT "'$filename' loaded...\n"
    if $break_on_load{$filename};
  print_lineinfo(' ' x $stack_depth, "Package $filename.\n") if $frame;
  return unless $postponed_file{$filename};
  $had_breakpoints{$filename} |= 1;
  #%dbline = %{$postponed_file{$filename}}; # Cannot be done: unsufficient magic
  my $key;
  for $key (keys %{$postponed_file{$filename}}) {
    $dbline{$key} = ${$postponed_file{$filename}}{$key};
  }
  delete $postponed_file{$filename};
}

sub dumpit {
    local ($savout) = select(shift);
    my $osingle = $single;
    my $otrace = $trace;
    $single = $trace = 0;
    local $frame = 0;
    local $doret = -2;
    unless (defined &main::dumpValue) {
	do 'dumpvar.pl';
    }
    if (defined &main::dumpValue) {
        local $\ = '';
        local $, = '';
        local $" = ' ';
        my $v = shift;
        my $maxdepth = shift || $option{dumpDepth};
        $maxdepth = -1 unless defined $maxdepth;   # -1 means infinite depth
	&main::dumpValue($v, $maxdepth);
    } else {
        local $\ = '';
	print $OUT "dumpvar.pl not available.\n";
    }
    $single = $osingle;
    $trace = $otrace;
    select ($savout);    
}

# Tied method do not create a context, so may get wrong message:

sub print_trace {
  local $\ = '';
  my $fh = shift;
  resetterm(1) if $fh eq $LINEINFO and $LINEINFO eq $OUT and $term_pid != $$;
  my @sub = dump_trace($_[0] + 1, $_[1]);
  my $short = $_[2];		# Print short report, next one for sub name
  my $s;
  for ($i=0; $i <= $#sub; $i++) {
    last if $signal;
    local $" = ', ';
    my $args = defined $sub[$i]{args} 
    ? "(@{ $sub[$i]{args} })"
      : '' ;
    $args = (substr $args, 0, $maxtrace - 3) . '...' 
      if length $args > $maxtrace;
    my $file = $sub[$i]{file};
    $file = $file eq '-e' ? $file : "file `$file'" unless $short;
    $s = $sub[$i]{sub};
    $s = (substr $s, 0, $maxtrace - 3) . '...' if length $s > $maxtrace;    
    if ($short) {
      my $sub = @_ >= 4 ? $_[3] : $s;
      print $fh "$sub[$i]{context}=$sub$args from $file:$sub[$i]{line}\n";
    } else {
      print $fh "$sub[$i]{context} = $s$args" .
	" called from $file" . 
	  " line $sub[$i]{line}\n";
    }
  }
}

sub dump_trace {
  my $skip = shift;
  my $count = shift || 1e9;
  $skip++;
  $count += $skip;
  my ($p,$file,$line,$sub,$h,$args,$e,$r,@a,@sub,$context);
  my $nothard = not $frame & 8;
  local $frame = 0;		# Do not want to trace this.
  my $otrace = $trace;
  $trace = 0;
  for ($i = $skip; 
       $i < $count and ($p,$file,$line,$sub,$h,$context,$e,$r) = caller($i); 
       $i++) {
    @a = ();
    for $arg (@args) {
      my $type;
      if (not defined $arg) {
	push @a, "undef";
      } elsif ($nothard and tied $arg) {
	push @a, "tied";
      } elsif ($nothard and $type = ref $arg) {
	push @a, "ref($type)";
      } else {
	local $_ = "$arg";	# Safe to stringify now - should not call f().
	s/([\'\\])/\\$1/g;
	s/(.*)/'$1'/s
	  unless /^(?: -?[\d.]+ | \*[\w:]* )$/x;
	s/([\200-\377])/sprintf("M-%c",ord($1)&0177)/eg;
	s/([\0-\37\177])/sprintf("^%c",ord($1)^64)/eg;
	push(@a, $_);
      }
    }
    $context = $context ? '@' : (defined $context ? "\$" : '.');
    $args = $h ? [@a] : undef;
    $e =~ s/\n\s*\;\s*\Z// if $e;
    $e =~ s/([\\\'])/\\$1/g if $e;
    if ($r) {
      $sub = "require '$e'";
    } elsif (defined $r) {
      $sub = "eval '$e'";
    } elsif ($sub eq '(eval)') {
      $sub = "eval {...}";
    }
    push(@sub, {context => $context, sub => $sub, args => $args,
		file => $file, line => $line});
    last if $signal;
  }
  $trace = $otrace;
  @sub;
}

sub action {
    my $action = shift;
    while ($action =~ s/\\$//) {
	#print $OUT "+ ";
	#$action .= "\n";
	$action .= &gets;
    }
    $action;
}

sub unbalanced { 
    # i hate using globals!
    $balanced_brace_re ||= qr{ 
	^ \{
	      (?:
		 (?> [^{}] + )    	    # Non-parens without backtracking
	       |
		 (??{ $balanced_brace_re }) # Group with matching parens
	      ) *
	  \} $
   }x;
   return $_[0] !~ m/$balanced_brace_re/;
}

sub gets {
    &readline("cont: ");
}

sub system {
    # We save, change, then restore STDIN and STDOUT to avoid fork() since
    # some non-Unix systems can do system() but have problems with fork().
    open(SAVEIN,"<&STDIN") || &warn("Can't save STDIN");
    open(SAVEOUT,">&STDOUT") || &warn("Can't save STDOUT");
    open(STDIN,"<&IN") || &warn("Can't redirect STDIN");
    open(STDOUT,">&OUT") || &warn("Can't redirect STDOUT");

    # XXX: using csh or tcsh destroys sigint retvals!
    system(@_);
    open(STDIN,"<&SAVEIN") || &warn("Can't restore STDIN");
    open(STDOUT,">&SAVEOUT") || &warn("Can't restore STDOUT");
    close(SAVEIN); 
    close(SAVEOUT);


    # most of the $? crud was coping with broken cshisms
    if ($? >> 8) {
	&warn("(Command exited ", ($? >> 8), ")\n");
    } elsif ($?) { 
	&warn( "(Command died of SIG#",  ($? & 127),
	    (($? & 128) ? " -- core dumped" : "") , ")", "\n");
    } 

    return $?;

}

sub setterm {
    local $frame = 0;
    local $doret = -2;
    eval { require Term::ReadLine } or die $@;
    if ($notty) {
	if ($tty) {
	    my ($i, $o) = split $tty, /,/;
	    $o = $i unless defined $o;
	    open(IN,"<$i") or die "Cannot open TTY `$i' for read: $!";
	    open(OUT,">$o") or die "Cannot open TTY `$o' for write: $!";
	    $IN = \*IN;
	    $OUT = \*OUT;
	    my $sel = select($OUT);
	    $| = 1;
	    select($sel);
	} else {
	    eval "require Term::Rendezvous;" or die;
	    my $rv = $ENV{PERLDB_NOTTY} || "/tmp/perldbtty$$";
	    my $term_rv = new Term::Rendezvous $rv;
	    $IN = $term_rv->IN;
	    $OUT = $term_rv->OUT;
	}
    }
    if ($term_pid eq '-1') {		# In a TTY with another debugger
	resetterm(2);
    }
    if (!$rl) {
	$term = new Term::ReadLine::Stub 'perldb', $IN, $OUT;
    } else {
	$term = new Term::ReadLine 'perldb', $IN, $OUT;

	$rl_attribs = $term->Attribs;
	$rl_attribs->{basic_word_break_characters} .= '-:+/*,[])}' 
	  if defined $rl_attribs->{basic_word_break_characters} 
	    and index($rl_attribs->{basic_word_break_characters}, ":") == -1;
	$rl_attribs->{special_prefixes} = '$@&%';
	$rl_attribs->{completer_word_break_characters} .= '$@&%';
	$rl_attribs->{completion_function} = \&db_complete; 
    }
    $LINEINFO = $OUT unless defined $LINEINFO;
    $lineinfo = $console unless defined $lineinfo;
    $term->MinLine(2);
    if ($term->Features->{setHistory} and "@hist" ne "?") {
      $term->SetHistory(@hist);
    }
    ornaments($ornaments) if defined $ornaments;
    $term_pid = $$;
}

# Example get_fork_TTY functions
sub xterm_get_fork_TTY {
  (my $name = $0) =~ s,^.*[/\\],,s;
  open XT, qq[3>&1 xterm -title "Daughter Perl debugger $pids $name" -e sh -c 'tty 1>&3;\
 sleep 10000000' |];
  my $tty = <XT>;
  chomp $tty;
  $pidprompt = '';		# Shown anyway in titlebar
  return $tty;
}

# This example function resets $IN, $OUT itself
sub os2_get_fork_TTY {
  local $^F = 40;			# XXXX Fixme!
  local $\ = '';
  my ($in1, $out1, $in2, $out2);
  # Having -d in PERL5OPT would lead to a disaster...
  local $ENV{PERL5OPT} = $ENV{PERL5OPT}    if $ENV{PERL5OPT};
  $ENV{PERL5OPT} =~ s/(?:^|(?<=\s))-d\b//  if $ENV{PERL5OPT};
  $ENV{PERL5OPT} =~ s/(?:^|(?<=\s))-d\B/-/ if $ENV{PERL5OPT};
  print $OUT "Making kid PERL5OPT->`$ENV{PERL5OPT}'.\n" if $ENV{PERL5OPT};
  local $ENV{PERL5LIB} = $ENV{PERL5LIB} ? $ENV{PERL5LIB} : $ENV{PERLLIB};
  $ENV{PERL5LIB} = '' unless defined $ENV{PERL5LIB};
  $ENV{PERL5LIB} = join ';', @ini_INC, split /;/, $ENV{PERL5LIB};
  (my $name = $0) =~ s,^.*[/\\],,s;
  my @args;
  if ( pipe $in1, $out1 and pipe $in2, $out2
       # system P_SESSION will fail if there is another process
       # in the same session with a "dependent" asynchronous child session.
       and @args = ($rl, fileno $in1, fileno $out2,
		    "Daughter Perl debugger $pids $name") and
       (($kpid = CORE::system 4, $^X, '-we', <<'ES', @args) >= 0 # P_SESSION
END {sleep 5 unless $loaded}
BEGIN {open STDIN,  '</dev/con' or warn "reopen stdin: $!"}
use OS2::Process;

my ($rl, $in) = (shift, shift);		# Read from $in and pass through
set_title pop;
system P_NOWAIT, $^X, '-we', <<EOS or die "Cannot start a grandkid";
  open IN, '<&=$in' or die "open <&=$in: \$!";
  \$| = 1; print while sysread IN, \$_, 1<<16;
EOS

my $out = shift;
open OUT, ">&=$out" or die "Cannot open &=$out for writing: $!";
select OUT;    $| = 1;
require Term::ReadKey if $rl;
Term::ReadKey::ReadMode(4) if $rl; # Nodelay on kbd.  Pipe is automatically nodelay...
print while sysread STDIN, $_, 1<<($rl ? 16 : 0);
ES
	 or warn "system P_SESSION: $!, $^E" and 0)
	and close $in1 and close $out2 ) {
      $pidprompt = '';			# Shown anyway in titlebar
      reset_IN_OUT($in2, $out1);
      $tty = '*reset*';
      return '';			# Indicate that reset_IN_OUT is called
   }
   return;
}

sub create_IN_OUT {	# Create a window with IN/OUT handles redirected there
    my $in = &get_fork_TTY if defined &get_fork_TTY;
    $in = $fork_TTY if defined $fork_TTY; # Backward compatibility
    if (not defined $in) {
      my $why = shift;
      print_help(<<EOP) if $why == 1;
I<#########> Forked, but do not know how to create a new B<TTY>. I<#########>
EOP
      print_help(<<EOP) if $why == 2;
I<#########> Daughter session, do not know how to change a B<TTY>. I<#########>
  This may be an asynchronous session, so the parent debugger may be active.
EOP
      print_help(<<EOP) if $why != 4;
  Since two debuggers fight for the same TTY, input is severely entangled.

EOP
      print_help(<<EOP);
  I know how to switch the output to a different window in xterms
  and OS/2 consoles only.  For a manual switch, put the name of the created I<TTY>
  in B<\$DB::fork_TTY>, or define a function B<DB::get_fork_TTY()> returning this.

  On I<UNIX>-like systems one can get the name of a I<TTY> for the given window
  by typing B<tty>, and disconnect the I<shell> from I<TTY> by B<sleep 1000000>.

EOP
    } elsif ($in ne '') {
      TTY($in);
    } else {
      $console = '';		# Indicate no need to open-from-the-console 
    }
    undef $fork_TTY;
}

sub resetterm {			# We forked, so we need a different TTY
    my $in = shift;
    my $systemed = $in > 1 ? '-' : '';
    if ($pids) {
      $pids =~ s/\]/$systemed->$$]/;
    } else {
      $pids = "[$term_pid->$$]";
    }
    $pidprompt = $pids;
    $term_pid = $$;
    return unless $CreateTTY & $in;
    create_IN_OUT($in);
}

sub readline {
  local $.;
  if (@typeahead) {
    my $left = @typeahead;
    my $got = shift @typeahead;
    local $\ = '';
    print $OUT "auto(-$left)", shift, $got, "\n";
    $term->AddHistory($got) 
      if length($got) > 1 and defined $term->Features->{addHistory};
    return $got;
  }
  local $frame = 0;
  local $doret = -2;
  while (@cmdfhs) {
    my $line = CORE::readline($cmdfhs[-1]);
    defined $line ? (print $OUT ">> $line" and return $line)
                  : close pop @cmdfhs;
  }
  if (ref $OUT and UNIVERSAL::isa($OUT, 'IO::Socket::INET')) {
    $OUT->write(join('', @_));
    my $stuff;
    $IN->recv( $stuff, 2048 );  # XXX: what's wrong with sysread?
    $stuff;
  }
  else {
    $term->readline(@_);
  }
}

sub dump_option {
    my ($opt, $val)= @_;
    $val = option_val($opt,'N/A');
    $val =~ s/([\\\'])/\\$1/g;
    printf $OUT "%20s = '%s'\n", $opt, $val;
}

sub option_val {
    my ($opt, $default)= @_;
    my $val;
    if (defined $optionVars{$opt}
	and defined ${$optionVars{$opt}}) {
	$val = ${$optionVars{$opt}};
    } elsif (defined $optionAction{$opt}
	and defined &{$optionAction{$opt}}) {
	$val = &{$optionAction{$opt}}();
    } elsif (defined $optionAction{$opt}
	     and not defined $option{$opt}
	     or defined $optionVars{$opt}
	     and not defined ${$optionVars{$opt}}) {
	$val = $default;
    } else {
	$val = $option{$opt};
    }
    $val = $default unless defined $val;
    $val
}

sub parse_options {
    local($_)= @_;
    local $\ = '';
    # too dangerous to let intuitive usage overwrite important things
    # defaultion should never be the default
    my %opt_needs_val = map { ( $_ => 1 ) } qw{
        dumpDepth arrayDepth hashDepth LineInfo maxTraceLen ornaments windowSize
        pager quote ReadLine recallCommand RemotePort ShellBang TTY
    };
    while (length) {
	my $val_defaulted;
	s/^\s+// && next;
	s/^(\w+)(\W?)// or print($OUT "Invalid option `$_'\n"), last;
	my ($opt,$sep) = ($1,$2);
	my $val;
	if ("?" eq $sep) {
	    print($OUT "Option query `$opt?' followed by non-space `$_'\n"), last
	      if /^\S/;
	    #&dump_option($opt);
	} elsif ($sep !~ /\S/) {
	    $val_defaulted = 1;
	    $val = "1";  #  this is an evil default; make 'em set it!
	} elsif ($sep eq "=") {
            if (s/ (["']) ( (?: \\. | (?! \1 ) [^\\] )* ) \1 //x) { 
                my $quote = $1;
                ($val = $2) =~ s/\\([$quote\\])/$1/g;
	    } else { 
		s/^(\S*)//;
	    $val = $1;
		print OUT qq(Option better cleared using $opt=""\n)
		    unless length $val;
	    }

	} else { #{ to "let some poor schmuck bounce on the % key in B<vi>."
	    my ($end) = "\\" . substr( ")]>}$sep", index("([<{",$sep), 1 ); #}
	    s/^(([^\\$end]|\\[\\$end])*)$end($|\s+)// or
	      print($OUT "Unclosed option value `$opt$sep$_'\n"), last;
	    ($val = $1) =~ s/\\([\\$end])/$1/g;
	}

	my $option;
	my $matches = grep( /^\Q$opt/  && ($option = $_),  @options  )
		   || grep( /^\Q$opt/i && ($option = $_),  @options  );

	print($OUT "Unknown option `$opt'\n"), next 	unless $matches;
	print($OUT "Ambiguous option `$opt'\n"), next 	if $matches > 1;

       if ($opt_needs_val{$option} && $val_defaulted) {
			 my $cmd = ($CommandSet eq '580') ? 'o' : 'O';
	    print $OUT "Option `$opt' is non-boolean.  Use `$cmd $option=VAL' to set, `$cmd $option?' to query\n";
	    next;
	} 

	$option{$option} = $val if defined $val;

	eval qq{
		local \$frame = 0; 
		local \$doret = -2; 
	        require '$optionRequire{$option}';
		1;
	 } || die  # XXX: shouldn't happen
	    if  defined $optionRequire{$option}	    &&
	        defined $val;

	${$optionVars{$option}} = $val 	    
	    if  defined $optionVars{$option}        &&
		defined $val;

	&{$optionAction{$option}} ($val)    
	    if defined $optionAction{$option}	    &&
               defined &{$optionAction{$option}}    &&
               defined $val;

	# Not $rcfile
	dump_option($option) 	unless $OUT eq \*STDERR; 
    }
}

sub set_list {
  my ($stem,@list) = @_;
  my $val;
  $ENV{"${stem}_n"} = @list;
  for $i (0 .. $#list) {
    $val = $list[$i];
    $val =~ s/\\/\\\\/g;
    $val =~ s/([\0-\37\177\200-\377])/"\\0x" . unpack('H2',$1)/eg;
    $ENV{"${stem}_$i"} = $val;
  }
}

sub get_list {
  my $stem = shift;
  my @list;
  my $n = delete $ENV{"${stem}_n"};
  my $val;
  for $i (0 .. $n - 1) {
    $val = delete $ENV{"${stem}_$i"};
    $val =~ s/\\((\\)|0x(..))/ $2 ? $2 : pack('H2', $3) /ge;
    push @list, $val;
  }
  @list;
}

sub catch {
    $signal = 1;
    return;			# Put nothing on the stack - malloc/free land!
}

sub warn {
    my($msg)= join("",@_);
    $msg .= ": $!\n" unless $msg =~ /\n$/;
    local $\ = '';
    print $OUT $msg;
}

sub reset_IN_OUT {
    my $switch_li = $LINEINFO eq $OUT;
    if ($term and $term->Features->{newTTY}) {
      ($IN, $OUT) = (shift, shift);
      $term->newTTY($IN, $OUT);
    } elsif ($term) {
	&warn("Too late to set IN/OUT filehandles, enabled on next `R'!\n");
    } else {
      ($IN, $OUT) = (shift, shift);
    }
    my $o = select $OUT;
    $| = 1;
    select $o;
    $LINEINFO = $OUT if $switch_li;
}

sub TTY {
    if (@_ and $term and $term->Features->{newTTY}) {
      my ($in, $out) = shift;
      if ($in =~ /,/) {
	($in, $out) = split /,/, $in, 2;
      } else {
	$out = $in;
      }
      open IN, $in or die "cannot open `$in' for read: $!";
      open OUT, ">$out" or die "cannot open `$out' for write: $!";
      reset_IN_OUT(\*IN,\*OUT);
      return $tty = $in;
    }
    &warn("Too late to set TTY, enabled on next `R'!\n") if $term and @_;
    # Useful if done through PERLDB_OPTS:
    $console = $tty = shift if @_;
    $tty or $console;
}

sub noTTY {
    if ($term) {
	&warn("Too late to set noTTY, enabled on next `R'!\n") if @_;
    }
    $notty = shift if @_;
    $notty;
}

sub ReadLine {
    if ($term) {
	&warn("Too late to set ReadLine, enabled on next `R'!\n") if @_;
    }
    $rl = shift if @_;
    $rl;
}

sub RemotePort {
    if ($term) {
        &warn("Too late to set RemotePort, enabled on next 'R'!\n") if @_;
    }
    $remoteport = shift if @_;
    $remoteport;
}

sub tkRunning {
    if (${$term->Features}{tkRunning}) {
        return $term->tkRunning(@_);
    } else {
	local $\ = '';
	print $OUT "tkRunning not supported by current ReadLine package.\n";
	0;
    }
}

sub NonStop {
    if ($term) {
	&warn("Too late to set up NonStop mode, enabled on next `R'!\n") if @_;
    }
    $runnonstop = shift if @_;
    $runnonstop;
}

sub pager {
    if (@_) {
	$pager = shift;
	$pager="|".$pager unless $pager =~ /^(\+?\>|\|)/;
    }
    $pager;
}

sub shellBang {
    if (@_) {
	$sh = quotemeta shift;
	$sh .= "\\b" if $sh =~ /\w$/;
    }
    $psh = $sh;
    $psh =~ s/\\b$//;
    $psh =~ s/\\(.)/$1/g;
    $psh;
}

sub ornaments {
  if (defined $term) {
    local ($warnLevel,$dieLevel) = (0, 1);
    return '' unless $term->Features->{ornaments};
    eval { $term->ornaments(@_) } || '';
  } else {
    $ornaments = shift;
  }
}

sub recallCommand {
    if (@_) {
	$rc = quotemeta shift;
	$rc .= "\\b" if $rc =~ /\w$/;
    }
    $prc = $rc;
    $prc =~ s/\\b$//;
    $prc =~ s/\\(.)/$1/g;
    $prc;
}

sub LineInfo {
    return $lineinfo unless @_;
    $lineinfo = shift;
    my $stream = ($lineinfo =~ /^(\+?\>|\|)/) ? $lineinfo : ">$lineinfo";
    $slave_editor = ($stream =~ /^\|/);
    open(LINEINFO, "$stream") || &warn("Cannot open `$stream' for write");
    $LINEINFO = \*LINEINFO;
    my $save = select($LINEINFO);
    $| = 1;
    select($save);
    $lineinfo;
}

sub list_modules { # versions
  my %version;
  my $file;
  for (keys %INC) {
    $file = $_;
    s,\.p[lm]$,,i ;
    s,/,::,g ;
    s/^perl5db$/DB/;
    s/^Term::ReadLine::readline$/readline/;
    if (defined ${ $_ . '::VERSION' }) {
      $version{$file} = "${ $_ . '::VERSION' } from ";
    } 
    $version{$file} .= $INC{$file};
  }
  dumpit($OUT,\%version);
}

sub sethelp {
    # XXX: make sure there are tabs between the command and explanation,
    #      or print_help will screw up your formatting if you have
    #      eeevil ornaments enabled.  This is an insane mess.

    $help = "
Help is currently only available for the new 580 CommandSet, 
if you really want old behaviour, presumably you know what 
you're doing ?-)

B<T>		Stack trace.
B<s> [I<expr>]	Single step [in I<expr>].
B<n> [I<expr>]	Next, steps over subroutine calls [in I<expr>].
<B<CR>>		Repeat last B<n> or B<s> command.
B<r>		Return from current subroutine.
B<c> [I<line>|I<sub>]	Continue; optionally inserts a one-time-only breakpoint
		at the specified position.
B<l> I<min>B<+>I<incr>	List I<incr>+1 lines starting at I<min>.
B<l> I<min>B<->I<max>	List lines I<min> through I<max>.
B<l> I<line>		List single I<line>.
B<l> I<subname>	List first window of lines from subroutine.
B<l> I<\$var>		List first window of lines from subroutine referenced by I<\$var>.
B<l>		List next window of lines.
B<->		List previous window of lines.
B<v> [I<line>]	View window around I<line>.
B<.>		Return to the executed line.
B<f> I<filename>	Switch to viewing I<filename>. File must be already loaded.
		I<filename> may be either the full name of the file, or a regular
		expression matching the full file name:
		B<f> I</home/me/foo.pl> and B<f> I<oo\\.> may access the same file.
		Evals (with saved bodies) are considered to be filenames:
		B<f> I<(eval 7)> and B<f> I<eval 7\\b> access the body of the 7th eval
		(in the order of execution).
B</>I<pattern>B</>	Search forwards for I<pattern>; final B</> is optional.
B<?>I<pattern>B<?>	Search backwards for I<pattern>; final B<?> is optional.
B<L> [I<a|b|w>]		List actions and or breakpoints and or watch-expressions.
B<S> [[B<!>]I<pattern>]	List subroutine names [not] matching I<pattern>.
B<t>		Toggle trace mode.
B<t> I<expr>		Trace through execution of I<expr>.
B<b>		Sets breakpoint on current line)
B<b> [I<line>] [I<condition>]
		Set breakpoint; I<line> defaults to the current execution line;
		I<condition> breaks if it evaluates to true, defaults to '1'.
B<b> I<subname> [I<condition>]
		Set breakpoint at first line of subroutine.
B<b> I<\$var>		Set breakpoint at first line of subroutine referenced by I<\$var>.
B<b> B<load> I<filename> Set breakpoint on 'require'ing the given file.
B<b> B<postpone> I<subname> [I<condition>]
		Set breakpoint at first line of subroutine after 
		it is compiled.
B<b> B<compile> I<subname>
		Stop after the subroutine is compiled.
B<B> [I<line>]	Delete the breakpoint for I<line>.
B<B> I<*>             Delete all breakpoints.
B<a> [I<line>] I<command>
		Set an action to be done before the I<line> is executed;
		I<line> defaults to the current execution line.
		Sequence is: check for breakpoint/watchpoint, print line
		if necessary, do action, prompt user if necessary,
		execute line.
B<a>		Does nothing
B<A> [I<line>]	Delete the action for I<line>.
B<A> I<*>             Delete all actions.
B<w> I<expr>		Add a global watch-expression.
B<w>     		Does nothing
B<W> I<expr>		Delete a global watch-expression.
B<W> I<*>             Delete all watch-expressions.
B<V> [I<pkg> [I<vars>]]	List some (default all) variables in package (default current).
		Use B<~>I<pattern> and B<!>I<pattern> for positive and negative regexps.
B<X> [I<vars>]	Same as \"B<V> I<currentpackage> [I<vars>]\".
B<x> I<expr>		Evals expression in list context, dumps the result.
B<m> I<expr>		Evals expression in list context, prints methods callable
		on the first element of the result.
B<m> I<class>		Prints methods callable via the given class.
B<M>		Show versions of loaded modules.

B<<> ?			List Perl commands to run before each prompt.
B<<> I<expr>		Define Perl command to run before each prompt.
B<<<> I<expr>		Add to the list of Perl commands to run before each prompt.
B<>> ?			List Perl commands to run after each prompt.
B<>> I<expr>		Define Perl command to run after each prompt.
B<>>B<>> I<expr>		Add to the list of Perl commands to run after each prompt.
B<{> I<db_command>	Define debugger command to run before each prompt.
B<{> ?			List debugger commands to run before each prompt.
B<{{> I<db_command>	Add to the list of debugger commands to run before each prompt.
B<$prc> I<number>	Redo a previous command (default previous command).
B<$prc> I<-number>	Redo number'th-to-last command.
B<$prc> I<pattern>	Redo last command that started with I<pattern>.
		See 'B<O> I<recallCommand>' too.
B<$psh$psh> I<cmd>  	Run cmd in a subprocess (reads from DB::IN, writes to DB::OUT)"
  . ( $rc eq $sh ? "" : "
B<$psh> [I<cmd>] 	Run I<cmd> in subshell (forces \"\$SHELL -c 'cmd'\")." ) . "
		See 'B<O> I<shellBang>' too.
B<source> I<file>		Execute I<file> containing debugger commands (may nest).
B<H> I<-number>	Display last number commands (default all).
B<p> I<expr>		Same as \"I<print {DB::OUT} expr>\" in current package.
B<|>I<dbcmd>		Run debugger command, piping DB::OUT to current pager.
B<||>I<dbcmd>		Same as B<|>I<dbcmd> but DB::OUT is temporarilly select()ed as well.
B<\=> [I<alias> I<value>]	Define a command alias, or list current aliases.
I<command>		Execute as a perl statement in current package.
B<R>		Pure-man-restart of debugger, some of debugger state
		and command-line options may be lost.
		Currently the following settings are preserved:
		history, breakpoints and actions, debugger B<O>ptions 
		and the following command-line options: I<-w>, I<-I>, I<-e>.

B<o> [I<opt>] ...	Set boolean option to true
B<o> [I<opt>B<?>]	Query options
B<o> [I<opt>B<=>I<val>] [I<opt>=B<\">I<val>B<\">] ... 
		Set options.  Use quotes in spaces in value.
    I<recallCommand>, I<ShellBang>	chars used to recall command or spawn shell;
    I<pager>			program for output of \"|cmd\";
    I<tkRunning>			run Tk while prompting (with ReadLine);
    I<signalLevel> I<warnLevel> I<dieLevel>	level of verbosity;
    I<inhibit_exit>		Allows stepping off the end of the script.
    I<ImmediateStop>		Debugger should stop as early as possible.
    I<RemotePort>			Remote hostname:port for remote debugging
  The following options affect what happens with B<V>, B<X>, and B<x> commands:
    I<arrayDepth>, I<hashDepth> 	print only first N elements ('' for all);
    I<compactDump>, I<veryCompact> 	change style of array and hash dump;
    I<globPrint> 			whether to print contents of globs;
    I<DumpDBFiles> 		dump arrays holding debugged files;
    I<DumpPackages> 		dump symbol tables of packages;
    I<DumpReused> 			dump contents of \"reused\" addresses;
    I<quote>, I<HighBit>, I<undefPrint> 	change style of string dump;
    I<bareStringify> 		Do not print the overload-stringified value;
  Other options include:
    I<PrintRet>		affects printing of return value after B<r> command,
    I<frame>		affects printing messages on subroutine entry/exit.
    I<AutoTrace>	affects printing messages on possible breaking points.
    I<maxTraceLen>	gives max length of evals/args listed in stack trace.
    I<ornaments> 	affects screen appearance of the command line.
    I<CreateTTY> 	bits control attempts to create a new TTY on events:
			1: on fork()	2: debugger is started inside debugger
			4: on startup
	During startup options are initialized from \$ENV{PERLDB_OPTS}.
	You can put additional initialization options I<TTY>, I<noTTY>,
	I<ReadLine>, I<NonStop>, and I<RemotePort> there (or use
	`B<R>' after you set them).

B<q> or B<^D>		Quit. Set B<\$DB::finished = 0> to debug global destruction.
B<h>		Summary of debugger commands.
B<h> [I<db_command>]	Get help [on a specific debugger command], enter B<|h> to page.
B<h h>		Long help for debugger commands
B<$doccmd> I<manpage>	Runs the external doc viewer B<$doccmd> command on the 
		named Perl I<manpage>, or on B<$doccmd> itself if omitted.
		Set B<\$DB::doccmd> to change viewer.

Type `|h h' for a paged display if this was too hard to read.

"; # Fix balance of vi % matching: }}}}

    #  note: tabs in the following section are not-so-helpful
    $summary = <<"END_SUM";
I<List/search source lines:>               I<Control script execution:>
  B<l> [I<ln>|I<sub>]  List source code            B<T>           Stack trace
  B<-> or B<.>      List previous/current line  B<s> [I<expr>]    Single step [in expr]
  B<v> [I<line>]    View around line            B<n> [I<expr>]    Next, steps over subs
  B<f> I<filename>  View source in file         <B<CR>/B<Enter>>  Repeat last B<n> or B<s>
  B</>I<pattern>B</> B<?>I<patt>B<?>   Search forw/backw    B<r>           Return from subroutine
  B<M>           Show module versions        B<c> [I<ln>|I<sub>]  Continue until position
I<Debugger controls:>                        B<L>           List break/watch/actions
  B<o> [...]     Set debugger options        B<t> [I<expr>]    Toggle trace [trace expr]
  B<<>[B<<>]|B<{>[B<{>]|B<>>[B<>>] [I<cmd>] Do pre/post-prompt B<b> [I<ln>|I<event>|I<sub>] [I<cnd>] Set breakpoint
  B<$prc> [I<N>|I<pat>]   Redo a previous command     B<B> I<ln|*>      Delete a/all breakpoints
  B<H> [I<-num>]    Display last num commands   B<a> [I<ln>] I<cmd>  Do cmd before line
  B<=> [I<a> I<val>]   Define/list an alias        B<A> I<ln|*>      Delete a/all actions
  B<h> [I<db_cmd>]  Get help on command         B<w> I<expr>      Add a watch expression
  B<h h>         Complete help page          B<W> I<expr|*>    Delete a/all watch exprs
  B<|>[B<|>]I<db_cmd>  Send output to pager        B<$psh>\[B<$psh>\] I<syscmd> Run cmd in a subprocess
  B<q> or B<^D>     Quit                        B<R>           Attempt a restart
I<Data Examination:>     B<expr>     Execute perl code, also see: B<s>,B<n>,B<t> I<expr>
  B<x>|B<m> I<expr>       Evals expr in list context, dumps the result or lists methods.
  B<p> I<expr>         Print expression (uses script's current package).
  B<S> [[B<!>]I<pat>]     List subroutine names [not] matching pattern
  B<V> [I<Pk> [I<Vars>]]  List Variables in Package.  Vars can be ~pattern or !pattern.
  B<X> [I<Vars>]       Same as \"B<V> I<current_package> [I<Vars>]\".
  B<y> [I<n> [I<Vars>]]   List lexicals in higher scope <n>.  Vars same as B<V>.
For more help, type B<h> I<cmd_letter>, or run B<$doccmd perldebug> for all docs.
END_SUM
				# ')}}; # Fix balance of vi % matching

	# and this is really numb...
	$pre580_help = "
B<T>		Stack trace.
B<s> [I<expr>]	Single step [in I<expr>].
B<n> [I<expr>]	Next, steps over subroutine calls [in I<expr>].
<B<CR>>		Repeat last B<n> or B<s> command.
B<r>		Return from current subroutine.
B<c> [I<line>|I<sub>]	Continue; optionally inserts a one-time-only breakpoint
		at the specified position.
B<l> I<min>B<+>I<incr>	List I<incr>+1 lines starting at I<min>.
B<l> I<min>B<->I<max>	List lines I<min> through I<max>.
B<l> I<line>		List single I<line>.
B<l> I<subname>	List first window of lines from subroutine.
B<l> I<\$var>		List first window of lines from subroutine referenced by I<\$var>.
B<l>		List next window of lines.
B<->		List previous window of lines.
B<w> [I<line>]	List window around I<line>.
B<.>		Return to the executed line.
B<f> I<filename>	Switch to viewing I<filename>. File must be already loaded.
		I<filename> may be either the full name of the file, or a regular
		expression matching the full file name:
		B<f> I</home/me/foo.pl> and B<f> I<oo\\.> may access the same file.
		Evals (with saved bodies) are considered to be filenames:
		B<f> I<(eval 7)> and B<f> I<eval 7\\b> access the body of the 7th eval
		(in the order of execution).
B</>I<pattern>B</>	Search forwards for I<pattern>; final B</> is optional.
B<?>I<pattern>B<?>	Search backwards for I<pattern>; final B<?> is optional.
B<L>		List all breakpoints and actions.
B<S> [[B<!>]I<pattern>]	List subroutine names [not] matching I<pattern>.
B<t>		Toggle trace mode.
B<t> I<expr>		Trace through execution of I<expr>.
B<b> [I<line>] [I<condition>]
		Set breakpoint; I<line> defaults to the current execution line;
		I<condition> breaks if it evaluates to true, defaults to '1'.
B<b> I<subname> [I<condition>]
		Set breakpoint at first line of subroutine.
B<b> I<\$var>		Set breakpoint at first line of subroutine referenced by I<\$var>.
B<b> B<load> I<filename> Set breakpoint on `require'ing the given file.
B<b> B<postpone> I<subname> [I<condition>]
		Set breakpoint at first line of subroutine after 
		it is compiled.
B<b> B<compile> I<subname>
		Stop after the subroutine is compiled.
B<d> [I<line>]	Delete the breakpoint for I<line>.
B<D>		Delete all breakpoints.
B<a> [I<line>] I<command>
		Set an action to be done before the I<line> is executed;
		I<line> defaults to the current execution line.
		Sequence is: check for breakpoint/watchpoint, print line
		if necessary, do action, prompt user if necessary,
		execute line.
B<a> [I<line>]	Delete the action for I<line>.
B<A>		Delete all actions.
B<W> I<expr>		Add a global watch-expression.
B<W>		Delete all watch-expressions.
B<V> [I<pkg> [I<vars>]]	List some (default all) variables in package (default current).
		Use B<~>I<pattern> and B<!>I<pattern> for positive and negative regexps.
B<X> [I<vars>]	Same as \"B<V> I<currentpackage> [I<vars>]\".
B<x> I<expr>		Evals expression in list context, dumps the result.
B<m> I<expr>		Evals expression in list context, prints methods callable
		on the first element of the result.
B<m> I<class>		Prints methods callable via the given class.

B<<> ?			List Perl commands to run before each prompt.
B<<> I<expr>		Define Perl command to run before each prompt.
B<<<> I<expr>		Add to the list of Perl commands to run before each prompt.
B<>> ?			List Perl commands to run after each prompt.
B<>> I<expr>		Define Perl command to run after each prompt.
B<>>B<>> I<expr>		Add to the list of Perl commands to run after each prompt.
B<{> I<db_command>	Define debugger command to run before each prompt.
B<{> ?			List debugger commands to run before each prompt.
B<{{> I<db_command>	Add to the list of debugger commands to run before each prompt.
B<$prc> I<number>	Redo a previous command (default previous command).
B<$prc> I<-number>	Redo number'th-to-last command.
B<$prc> I<pattern>	Redo last command that started with I<pattern>.
		See 'B<O> I<recallCommand>' too.
B<$psh$psh> I<cmd>  	Run cmd in a subprocess (reads from DB::IN, writes to DB::OUT)"
  . ( $rc eq $sh ? "" : "
B<$psh> [I<cmd>] 	Run I<cmd> in subshell (forces \"\$SHELL -c 'cmd'\")." ) . "
		See 'B<O> I<shellBang>' too.
B<source> I<file>		Execute I<file> containing debugger commands (may nest).
B<H> I<-number>	Display last number commands (default all).
B<p> I<expr>		Same as \"I<print {DB::OUT} expr>\" in current package.
B<|>I<dbcmd>		Run debugger command, piping DB::OUT to current pager.
B<||>I<dbcmd>		Same as B<|>I<dbcmd> but DB::OUT is temporarilly select()ed as well.
B<\=> [I<alias> I<value>]	Define a command alias, or list current aliases.
I<command>		Execute as a perl statement in current package.
B<v>		Show versions of loaded modules.
B<R>		Pure-man-restart of debugger, some of debugger state
		and command-line options may be lost.
		Currently the following settings are preserved:
		history, breakpoints and actions, debugger B<O>ptions 
		and the following command-line options: I<-w>, I<-I>, I<-e>.

B<O> [I<opt>] ...	Set boolean option to true
B<O> [I<opt>B<?>]	Query options
B<O> [I<opt>B<=>I<val>] [I<opt>=B<\">I<val>B<\">] ... 
		Set options.  Use quotes in spaces in value.
    I<recallCommand>, I<ShellBang>	chars used to recall command or spawn shell;
    I<pager>			program for output of \"|cmd\";
    I<tkRunning>			run Tk while prompting (with ReadLine);
    I<signalLevel> I<warnLevel> I<dieLevel>	level of verbosity;
    I<inhibit_exit>		Allows stepping off the end of the script.
    I<ImmediateStop>		Debugger should stop as early as possible.
    I<RemotePort>			Remote hostname:port for remote debugging
  The following options affect what happens with B<V>, B<X>, and B<x> commands:
    I<arrayDepth>, I<hashDepth> 	print only first N elements ('' for all);
    I<compactDump>, I<veryCompact> 	change style of array and hash dump;
    I<globPrint> 			whether to print contents of globs;
    I<DumpDBFiles> 		dump arrays holding debugged files;
    I<DumpPackages> 		dump symbol tables of packages;
    I<DumpReused> 			dump contents of \"reused\" addresses;
    I<quote>, I<HighBit>, I<undefPrint> 	change style of string dump;
    I<bareStringify> 		Do not print the overload-stringified value;
  Other options include:
    I<PrintRet>		affects printing of return value after B<r> command,
    I<frame>		affects printing messages on subroutine entry/exit.
    I<AutoTrace>	affects printing messages on possible breaking points.
    I<maxTraceLen>	gives max length of evals/args listed in stack trace.
    I<ornaments> 	affects screen appearance of the command line.
    I<CreateTTY> 	bits control attempts to create a new TTY on events:
			1: on fork()	2: debugger is started inside debugger
			4: on startup
	During startup options are initialized from \$ENV{PERLDB_OPTS}.
	You can put additional initialization options I<TTY>, I<noTTY>,
	I<ReadLine>, I<NonStop>, and I<RemotePort> there (or use
	`B<R>' after you set them).

B<q> or B<^D>		Quit. Set B<\$DB::finished = 0> to debug global destruction.
B<h> [I<db_command>]	Get help [on a specific debugger command], enter B<|h> to page.
B<h h>		Summary of debugger commands.
B<$doccmd> I<manpage>	Runs the external doc viewer B<$doccmd> command on the 
		named Perl I<manpage>, or on B<$doccmd> itself if omitted.
		Set B<\$DB::doccmd> to change viewer.

Type `|h' for a paged display if this was too hard to read.

"; # Fix balance of vi % matching: }}}}

    #  note: tabs in the following section are not-so-helpful
    $pre580_summary = <<"END_SUM";
I<List/search source lines:>               I<Control script execution:>
  B<l> [I<ln>|I<sub>]  List source code            B<T>           Stack trace
  B<-> or B<.>      List previous/current line  B<s> [I<expr>]    Single step [in expr]
  B<w> [I<line>]    List around line            B<n> [I<expr>]    Next, steps over subs
  B<f> I<filename>  View source in file         <B<CR>/B<Enter>>  Repeat last B<n> or B<s>
  B</>I<pattern>B</> B<?>I<patt>B<?>   Search forw/backw    B<r>           Return from subroutine
  B<v>           Show versions of modules    B<c> [I<ln>|I<sub>]  Continue until position
I<Debugger controls:>                        B<L>           List break/watch/actions
  B<O> [...]     Set debugger options        B<t> [I<expr>]    Toggle trace [trace expr]
  B<<>[B<<>]|B<{>[B<{>]|B<>>[B<>>] [I<cmd>] Do pre/post-prompt B<b> [I<ln>|I<event>|I<sub>] [I<cnd>] Set breakpoint
  B<$prc> [I<N>|I<pat>]   Redo a previous command     B<d> [I<ln>] or B<D> Delete a/all breakpoints
  B<H> [I<-num>]    Display last num commands   B<a> [I<ln>] I<cmd>  Do cmd before line
  B<=> [I<a> I<val>]   Define/list an alias        B<W> I<expr>      Add a watch expression
  B<h> [I<db_cmd>]  Get help on command         B<A> or B<W>      Delete all actions/watch
  B<|>[B<|>]I<db_cmd>  Send output to pager        B<$psh>\[B<$psh>\] I<syscmd> Run cmd in a subprocess
  B<q> or B<^D>     Quit                        B<R>           Attempt a restart
I<Data Examination:>     B<expr>     Execute perl code, also see: B<s>,B<n>,B<t> I<expr>
  B<x>|B<m> I<expr>       Evals expr in list context, dumps the result or lists methods.
  B<p> I<expr>         Print expression (uses script's current package).
  B<S> [[B<!>]I<pat>]     List subroutine names [not] matching pattern
  B<V> [I<Pk> [I<Vars>]]  List Variables in Package.  Vars can be ~pattern or !pattern.
  B<X> [I<Vars>]       Same as \"B<V> I<current_package> [I<Vars>]\".
  B<y> [I<n> [I<Vars>]]   List lexicals in higher scope <n>.  Vars same as B<V>.
For more help, type B<h> I<cmd_letter>, or run B<$doccmd perldebug> for all docs.
END_SUM
				# ')}}; # Fix balance of vi % matching

}

sub print_help {
    local $_ = shift;

    # Restore proper alignment destroyed by eeevil I<> and B<>
    # ornaments: A pox on both their houses!
    #
    # A help command will have everything up to and including
    # the first tab sequence padded into a field 16 (or if indented 20)
    # wide.  If it's wider than that, an extra space will be added.
    s{
	^ 		    	# only matters at start of line
	  ( \040{4} | \t )*	# some subcommands are indented
	  ( < ? 		# so <CR> works
	    [BI] < [^\t\n] + )  # find an eeevil ornament
	  ( \t+ )		# original separation, discarded
	  ( .* )		# this will now start (no earlier) than 
				# column 16
    } {
	my($leadwhite, $command, $midwhite, $text) = ($1, $2, $3, $4);
	my $clean = $command;
	$clean =~ s/[BI]<([^>]*)>/$1/g;  
    # replace with this whole string:
	($leadwhite ? " " x 4 : "")
      . $command
      . ((" " x (16 + ($leadwhite ? 4 : 0) - length($clean))) || " ")
      . $text;

    }mgex;

    s{				# handle bold ornaments
	B < ( [^>] + | > ) >
    } {
	  $Term::ReadLine::TermCap::rl_term_set[2] 
	. $1
	. $Term::ReadLine::TermCap::rl_term_set[3]
    }gex;

    s{				# handle italic ornaments
	I < ( [^>] + | > ) >
    } {
	  $Term::ReadLine::TermCap::rl_term_set[0] 
	. $1
	. $Term::ReadLine::TermCap::rl_term_set[1]
    }gex;

    local $\ = '';
    print $OUT $_;
}

sub fix_less {
    return if defined $ENV{LESS} && $ENV{LESS} =~ /r/;
    my $is_less = $pager =~ /\bless\b/;
    if ($pager =~ /\bmore\b/) { 
	my @st_more = stat('/usr/bin/more');
	my @st_less = stat('/usr/bin/less');
	$is_less = @st_more    && @st_less 
		&& $st_more[0] == $st_less[0] 
		&& $st_more[1] == $st_less[1];
    }
    # changes environment!
    $ENV{LESS} .= 'r' 	if $is_less;
}

sub diesignal {
    local $frame = 0;
    local $doret = -2;
    $SIG{'ABRT'} = 'DEFAULT';
    kill 'ABRT', $$ if $panic++;
    if (defined &Carp::longmess) {
	local $SIG{__WARN__} = '';
	local $Carp::CarpLevel = 2;		# mydie + confess
	&warn(Carp::longmess("Signal @_"));
    }
    else {
	local $\ = '';
	print $DB::OUT "Got signal @_\n";
    }
    kill 'ABRT', $$;
}

sub dbwarn { 
  local $frame = 0;
  local $doret = -2;
  local $SIG{__WARN__} = '';
  local $SIG{__DIE__} = '';
  eval { require Carp } if defined $^S;	# If error/warning during compilation,
                                        # require may be broken.
  CORE::warn(@_, "\nCannot print stack trace, load with -MCarp option to see stack"),
    return unless defined &Carp::longmess;
  my ($mysingle,$mytrace) = ($single,$trace);
  $single = 0; $trace = 0;
  my $mess = Carp::longmess(@_);
  ($single,$trace) = ($mysingle,$mytrace);
  &warn($mess); 
}

sub dbdie {
  local $frame = 0;
  local $doret = -2;
  local $SIG{__DIE__} = '';
  local $SIG{__WARN__} = '';
  my $i = 0; my $ineval = 0; my $sub;
  if ($dieLevel > 2) {
      local $SIG{__WARN__} = \&dbwarn;
      &warn(@_);		# Yell no matter what
      return;
  }
  if ($dieLevel < 2) {
    die @_ if $^S;		# in eval propagate
  }
  # No need to check $^S, eval is much more robust nowadays
  eval { require Carp }; #if defined $^S;# If error/warning during compilation,
                                	# require may be broken.

  die(@_, "\nCannot print stack trace, load with -MCarp option to see stack")
    unless defined &Carp::longmess;

  # We do not want to debug this chunk (automatic disabling works
  # inside DB::DB, but not in Carp).
  my ($mysingle,$mytrace) = ($single,$trace);
  $single = 0; $trace = 0;
  my $mess = "@_";
  { 
    package Carp;		# Do not include us in the list
    eval {
      $mess = Carp::longmess(@_);
    };
  }
  ($single,$trace) = ($mysingle,$mytrace);
  die $mess;
}

sub warnLevel {
  if (@_) {
    $prevwarn = $SIG{__WARN__} unless $warnLevel;
    $warnLevel = shift;
    if ($warnLevel) {
      $SIG{__WARN__} = \&DB::dbwarn;
    } elsif ($prevwarn) {
      $SIG{__WARN__} = $prevwarn;
    }
  }
  $warnLevel;
}

sub dieLevel {
  local $\ = '';
  if (@_) {
    $prevdie = $SIG{__DIE__} unless $dieLevel;
    $dieLevel = shift;
    if ($dieLevel) {
      $SIG{__DIE__} = \&DB::dbdie; # if $dieLevel < 2;
      #$SIG{__DIE__} = \&DB::diehard if $dieLevel >= 2;
      print $OUT "Stack dump during die enabled", 
        ( $dieLevel == 1 ? " outside of evals" : ""), ".\n"
	  if $I_m_init;
      print $OUT "Dump printed too.\n" if $dieLevel > 2;
    } elsif ($prevdie) {
      $SIG{__DIE__} = $prevdie;
      print $OUT "Default die handler restored.\n";
    }
  }
  $dieLevel;
}

sub signalLevel {
  if (@_) {
    $prevsegv = $SIG{SEGV} unless $signalLevel;
    $prevbus = $SIG{BUS} unless $signalLevel;
    $signalLevel = shift;
    if ($signalLevel) {
      $SIG{SEGV} = \&DB::diesignal;
      $SIG{BUS} = \&DB::diesignal;
    } else {
      $SIG{SEGV} = $prevsegv;
      $SIG{BUS} = $prevbus;
    }
  }
  $signalLevel;
}

sub CvGV_name {
  my $in = shift;
  my $name = CvGV_name_or_bust($in);
  defined $name ? $name : $in;
}

sub CvGV_name_or_bust {
  my $in = shift;
  return if $skipCvGV;		# Backdoor to avoid problems if XS broken...
  return unless ref $in;
  $in = \&$in;			# Hard reference...
  eval {require Devel::Peek; 1} or return;
  my $gv = Devel::Peek::CvGV($in) or return;
  *$gv{PACKAGE} . '::' . *$gv{NAME};
}

sub find_sub {
  my $subr = shift;
  $sub{$subr} or do {
    return unless defined &$subr;
    my $name = CvGV_name_or_bust($subr);
    my $data;
    $data = $sub{$name} if defined $name;
    return $data if defined $data;

    # Old stupid way...
    $subr = \&$subr;		# Hard reference
    my $s;
    for (keys %sub) {
      $s = $_, last if $subr eq \&$_;
    }
    $sub{$s} if $s;
  }
}

sub methods {
  my $class = shift;
  $class = ref $class if ref $class;
  local %seen;
  local %packs;
  methods_via($class, '', 1);
  methods_via('UNIVERSAL', 'UNIVERSAL', 0);
}

sub methods_via {
  my $class = shift;
  return if $packs{$class}++;
  my $prefix = shift;
  my $prepend = $prefix ? "via $prefix: " : '';
  my $name;
  for $name (grep {defined &{${"${class}::"}{$_}}} 
	     sort keys %{"${class}::"}) {
    next if $seen{ $name }++;
    local $\ = '';
    local $, = '';
    print $DB::OUT "$prepend$name\n";
  }
  return unless shift;		# Recurse?
  for $name (@{"${class}::ISA"}) {
    $prepend = $prefix ? $prefix . " -> $name" : $name;
    methods_via($name, $prepend, 1);
  }
}

sub setman { 
    $doccmd = $^O !~ /^(?:MSWin32|VMS|os2|dos|amigaos|riscos|MacOS|NetWare)\z/s
		? "man"             # O Happy Day!
		: "perldoc";        # Alas, poor unfortunates
}

sub runman {
    my $page = shift;
    unless ($page) {
	&system("$doccmd $doccmd");
	return;
    } 
    # this way user can override, like with $doccmd="man -Mwhatever"
    # or even just "man " to disable the path check.
    unless ($doccmd eq 'man') {
	&system("$doccmd $page");
	return;
    } 

    $page = 'perl' if lc($page) eq 'help';

    require Config;
    my $man1dir = $Config::Config{'man1dir'};
    my $man3dir = $Config::Config{'man3dir'};
    for ($man1dir, $man3dir) { s#/[^/]*\z## if /\S/ } 
    my $manpath = '';
    $manpath .= "$man1dir:" if $man1dir =~ /\S/;
    $manpath .= "$man3dir:" if $man3dir =~ /\S/ && $man1dir ne $man3dir;
    chop $manpath if $manpath;
    # harmless if missing, I figure
    my $oldpath = $ENV{MANPATH};
    $ENV{MANPATH} = $manpath if $manpath;
    my $nopathopt = $^O =~ /dunno what goes here/;
    if (CORE::system($doccmd, 
		# I just *know* there are men without -M
		(($manpath && !$nopathopt) ? ("-M", $manpath) : ()),  
	    split ' ', $page) )
    {
	unless ($page =~ /^perl\w/) {
	    if (grep { $page eq $_ } qw{ 
		5004delta 5005delta amiga api apio book boot bot call compile
		cygwin data dbmfilter debug debguts delta diag doc dos dsc embed
		faq faq1 faq2 faq3 faq4 faq5 faq6 faq7 faq8 faq9 filter fork
		form func guts hack hist hpux intern ipc lexwarn locale lol mod
		modinstall modlib number obj op opentut os2 os390 pod port 
		ref reftut run sec style sub syn thrtut tie toc todo toot tootc
		trap unicode var vms win32 xs xstut
	      }) 
	    {
		$page =~ s/^/perl/;
		CORE::system($doccmd, 
			(($manpath && !$nopathopt) ? ("-M", $manpath) : ()),  
			$page);
	    }
	}
    } 
    if (defined $oldpath) {
	$ENV{MANPATH} = $manpath;
    } else {
	delete $ENV{MANPATH};
    } 
} 

# The following BEGIN is very handy if debugger goes havoc, debugging debugger?

BEGIN {			# This does not compile, alas.
  $IN = \*STDIN;		# For bugs before DB::OUT has been opened
  $OUT = \*STDERR;		# For errors before DB::OUT has been opened
  $sh = '!';
  $rc = ',';
  @hist = ('?');
  $deep = 100;			# warning if stack gets this deep
  $window = 10;
  $preview = 3;
  $sub = '';
  $SIG{INT} = \&DB::catch;
  # This may be enabled to debug debugger:
  #$warnLevel = 1 unless defined $warnLevel;
  #$dieLevel = 1 unless defined $dieLevel;
  #$signalLevel = 1 unless defined $signalLevel;

  $db_stop = 0;			# Compiler warning
  $db_stop = 1 << 30;
  $level = 0;			# Level of recursive debugging
  # @stack and $doret are needed in sub sub, which is called for DB::postponed.
  # Triggers bug (?) in perl is we postpone this until runtime:
  @postponed = @stack = (0);
  $stack_depth = 0;		# Localized $#stack
  $doret = -2;
  $frame = 0;
}

BEGIN {$^W = $ini_warn;}	# Switch warnings back

#use Carp;			# This did break, left for debugging

sub db_complete {
  # Specific code for b c l V m f O, &blah, $blah, @blah, %blah
  my($text, $line, $start) = @_;
  my ($itext, $search, $prefix, $pack) =
    ($text, "^\Q${'package'}::\E([^:]+)\$");
  
  return sort grep /^\Q$text/, (keys %sub), qw(postpone load compile), # subroutines
                               (map { /$search/ ? ($1) : () } keys %sub)
    if (substr $line, 0, $start) =~ /^\|*[blc]\s+((postpone|compile)\s+)?$/;
  return sort grep /^\Q$text/, values %INC # files
    if (substr $line, 0, $start) =~ /^\|*b\s+load\s+$/;
  return sort map {($_, db_complete($_ . "::", "V ", 2))}
    grep /^\Q$text/, map { /^(.*)::$/ ? ($1) : ()} keys %:: # top-packages
      if (substr $line, 0, $start) =~ /^\|*[Vm]\s+$/ and $text =~ /^\w*$/;
  return sort map {($_, db_complete($_ . "::", "V ", 2))}
    grep !/^main::/,
      grep /^\Q$text/, map { /^(.*)::$/ ? ($prefix . "::$1") : ()} keys %{$prefix . '::'}
				 # packages
	if (substr $line, 0, $start) =~ /^\|*[Vm]\s+$/ 
	  and $text =~ /^(.*[^:])::?(\w*)$/  and $prefix = $1;
  if ( $line =~ /^\|*f\s+(.*)/ ) { # Loaded files
    # We may want to complete to (eval 9), so $text may be wrong
    $prefix = length($1) - length($text);
    $text = $1;
    return sort 
	map {substr $_, 2 + $prefix} grep /^_<\Q$text/, (keys %main::), $0
  }
  if ((substr $text, 0, 1) eq '&') { # subroutines
    $text = substr $text, 1;
    $prefix = "&";
    return sort map "$prefix$_", 
               grep /^\Q$text/, 
                 (keys %sub),
                 (map { /$search/ ? ($1) : () } 
		    keys %sub);
  }
  if ($text =~ /^[\$@%](.*)::(.*)/) { # symbols in a package
    $pack = ($1 eq 'main' ? '' : $1) . '::';
    $prefix = (substr $text, 0, 1) . $1 . '::';
    $text = $2;
    my @out 
      = map "$prefix$_", grep /^\Q$text/, grep /^_?[a-zA-Z]/, keys %$pack ;
    if (@out == 1 and $out[0] =~ /::$/ and $out[0] ne $itext) {
      return db_complete($out[0], $line, $start);
    }
    return sort @out;
  }
  if ($text =~ /^[\$@%]/) { # symbols (in $package + packages in main)
    $pack = ($package eq 'main' ? '' : $package) . '::';
    $prefix = substr $text, 0, 1;
    $text = substr $text, 1;
    my @out = map "$prefix$_", grep /^\Q$text/, 
       (grep /^_?[a-zA-Z]/, keys %$pack), 
       ( $pack eq '::' ? () : (grep /::$/, keys %::) ) ;
    if (@out == 1 and $out[0] =~ /::$/ and $out[0] ne $itext) {
      return db_complete($out[0], $line, $start);
    }
    return sort @out;
  }
  if ((substr $line, 0, $start) =~ /^\|*O\b.*\s$/) { # Options after a space
    my @out = grep /^\Q$text/, @options;
    my $val = option_val($out[0], undef);
    my $out = '? ';
    if (not defined $val or $val =~ /[\n\r]/) {
      # Can do nothing better
    } elsif ($val =~ /\s/) {
      my $found;
      foreach $l (split //, qq/\"\'\#\|/) {
	$out = "$l$val$l ", last if (index $val, $l) == -1;
      }
    } else {
      $out = "=$val ";
    }
    # Default to value if one completion, to question if many
    $rl_attribs->{completer_terminator_character} = (@out == 1 ? $out : '? ');
    return sort @out;
  }
  return $term->filename_list($text); # filenames
}

sub end_report {
  local $\ = '';
  print $OUT "Use `q' to quit or `R' to restart.  `h q' for details.\n"
}

sub clean_ENV {
    if (defined($ini_pids)) {
        $ENV{PERLDB_PIDS} = $ini_pids;
    } else {
        delete($ENV{PERLDB_PIDS});
    }
}

END {
  $finished = 1 if $inhibit_exit;      # So that some keys may be disabled.
  $fall_off_end = 1 unless $inhibit_exit;
  # Do not stop in at_exit() and destructors on exit:
  $DB::single = !$fall_off_end && !$runnonstop;
  DB::fake::at_exit() unless $fall_off_end or $runnonstop;
}


# ===================================== pre580 ================================
# this is very sad below here...
#

sub cmd_pre580_null {
	# do nothing...
}

sub cmd_pre580_a {
	my $cmd = shift;
	if ($cmd =~ /^(\d*)\s*(.*)/) {
		$i = $1 || $line; $j = $2;
		if (length $j) {
			if ($dbline[$i] == 0) {
				print $OUT "Line $i may not have an action.\n";
			} else {
				$had_breakpoints{$filename} |= 2;
				$dbline{$i} =~ s/\0[^\0]*//;
				$dbline{$i} .= "\0" . action($j);
			}
		} else {
			$dbline{$i} =~ s/\0[^\0]*//;
			delete $dbline{$i} if $dbline{$i} eq '';
		}
	}
}

sub cmd_pre580_b {
	my $cmd    = shift;
	my $dbline = shift;
	if ($cmd =~ /^load\b\s*(.*)/) {
		my $file = $1; $file =~ s/\s+$//;
		&cmd_b_load($file);
	} elsif ($cmd =~ /^(postpone|compile)\b\s*([':A-Za-z_][':\w]*)\s*(.*)/) {
		my $cond = length $3 ? $3 : '1';
		my ($subname, $break) = ($2, $1 eq 'postpone');
		$subname =~ s/\'/::/g;
		$subname = "${'package'}::" . $subname
		unless $subname =~ /::/;
		$subname = "main".$subname if substr($subname,0,2) eq "::";
		$postponed{$subname} = $break ? "break +0 if $cond" : "compile";
	} elsif ($cmd =~ /^([':A-Za-z_][':\w]*(?:\[.*\])?)\s*(.*)/) { 
		my $subname = $1;
		my $cond = length $2 ? $2 : '1';
		&cmd_b_sub($subname, $cond);
	} elsif ($cmd =~ /^(\d*)\s*(.*)/) {
		my $i = $1 || $dbline;
		my $cond = length $2 ? $2 : '1';
		&cmd_b_line($i, $cond);
	}
}

sub cmd_pre580_D {
	my $cmd = shift;
	if ($cmd =~ /^\s*$/) {
		print $OUT "Deleting all breakpoints...\n";
		my $file;
		for $file (keys %had_breakpoints) {
			local *dbline = $main::{'_<' . $file};
			my $max = $#dbline;
			my $was;

			for ($i = 1; $i <= $max ; $i++) {
				if (defined $dbline{$i}) {
					$dbline{$i} =~ s/^[^\0]+//;
					if ($dbline{$i} =~ s/^\0?$//) {
						delete $dbline{$i};
					}
				}
			}

			if (not $had_breakpoints{$file} &= ~1) {
				delete $had_breakpoints{$file};
			}
		}
		undef %postponed;
		undef %postponed_file;
		undef %break_on_load;
	}
}

sub cmd_pre580_h {
	my $cmd = shift;
	if ($cmd =~ /^\s*$/) {
		print_help($pre580_help);
	} elsif ($cmd =~ /^h\s*/) {
		print_help($pre580_summary);
	} elsif ($cmd =~ /^h\s+(\S.*)$/) { 
		my $asked = $1;			# for proper errmsg
		my $qasked = quotemeta($asked); # for searching
		# XXX: finds CR but not <CR>
		if ($pre580_help =~ /^<?(?:[IB]<)$qasked/m) {
			while ($pre580_help =~ /^(<?(?:[IB]<)$qasked([\s\S]*?)\n)(?!\s)/mg) {
				print_help($1);
			}
		} else {
			print_help("B<$asked> is not a debugger command.\n");
		}
	}
}

sub cmd_pre580_W {
	my $cmd = shift;
	if ($cmd =~ /^$/) { 
		$trace &= ~2;
		@to_watch = @old_watch = ();
	} elsif ($cmd =~ /^(.*)/s) {
		push @to_watch, $1;
		$evalarg = $1;
		my ($val) = &eval;
		$val = (defined $val) ? "'$val'" : 'undef' ;
		push @old_watch, $val;
		$trace |= 2;
	}
}

package DB::fake;

sub at_exit {
  "Debugged program terminated.  Use `q' to quit or `R' to restart.";
}

package DB;			# Do not trace this 1; below!

1;

