#./perl -w
#
# Create the export list for perl.
#
# Needed by WIN32 and OS/2 for creating perl.dll,
# and by AIX for creating libperl.a when -Duseshrplib is in effect,
# and by VMS for creating perlshr.exe.
#
# Reads from information stored in
#
#    %Config::Config (ie config.sh)
#    config.h
#    embed.fnc
#    globvar.sym
#    intrpvar.h
#    miniperl.map (on OS/2)
#    perl5.def    (on OS/2; this is the old version of the file being made)
#    perlio.sym
#    perlvars.h
#    regen/opcodes
#
# plus long lists of function names hard-coded directly in this script.
#
# Writes the result to STDOUT.
#
# Normally this script is invoked from a makefile (e.g. win32/Makefile),
# which redirects STDOUT to a suitable file, such as:
#
#    perl5.def   OS/2
#    perldll.def Windows
#    perl.exp    AIX
#    perl.imp    NetWare
#    makedef.lis VMS

BEGIN { unshift @INC, "lib" }
use Config;
use strict;

my %ARGS = (CCTYPE => 'MSVC', TARG_DIR => '');

my %define;

my $fold;

sub process_cc_flags {
    foreach (map {split /\s+/, $_} @_) {
	$define{$1} = $2 // 1 if /^-D(\w+)(?:=(.+))?/;
    }
}

while (@ARGV) {
    my $flag = shift;
    if ($flag =~ /^(?:CC_FLAGS=)?(-D\w.*)/) {
	process_cc_flags($1);
    } elsif ($flag =~ /^(CCTYPE|FILETYPE|PLATFORM|TARG_DIR)=(.+)$/) {
	$ARGS{$1} = $2;
    } elsif ($flag eq '--sort-fold') {
	++$fold;
    }
}

require "$ARGS{TARG_DIR}regen/embed_lib.pl";

{
    my @PLATFORM = qw(aix win32 wince os2 netware vms test);
    my %PLATFORM;
    @PLATFORM{@PLATFORM} = ();

    die "PLATFORM undefined, must be one of: @PLATFORM\n"
	unless defined $ARGS{PLATFORM};
    die "PLATFORM must be one of: @PLATFORM\n"
	unless exists $PLATFORM{$ARGS{PLATFORM}};
}

# Is the following guard strictly necessary? Added during refactoring
# to keep the same behaviour when merging other code into here.
process_cc_flags(@Config{qw(ccflags optimize)})
    if $ARGS{PLATFORM} ne 'win32' && $ARGS{PLATFORM} ne 'wince'
    && $ARGS{PLATFORM} ne 'netware';

# Add the compile-time options that miniperl was built with to %define.
# On Win32 these are not the same options as perl itself will be built
# with since miniperl is built with a canned config (one of the win32/
# config_H.*) and none of the BUILDOPT's that are set in the makefiles,
# but they do include some #define's that are hard-coded in various
# source files and header files and don't include any BUILDOPT's that
# the user might have chosen to disable because the canned configs are
# minimal configs that don't include any of those options.

#don't use the host Perl's -V defines for the WinCE Perl
if($ARGS{PLATFORM} ne 'wince') {
    my @options = sort(Config::bincompat_options(), Config::non_bincompat_options());
    print STDERR "Options: (@options)\n" unless $ARGS{PLATFORM} eq 'test';
    $define{$_} = 1 foreach @options;
}

my %exportperlmalloc =
    (
       Perl_malloc		=>	"malloc",
       Perl_mfree		=>	"free",
       Perl_realloc		=>	"realloc",
       Perl_calloc		=>	"calloc",
    );

my $exportperlmalloc = $ARGS{PLATFORM} eq 'os2';

my $config_h = 'config.h';
open(CFG, '<', $config_h) || die "Cannot open $config_h: $!\n";
while (<CFG>) {
    $define{$1} = 1 if /^\s*\#\s*define\s+(MYMALLOC|MULTIPLICITY
                                           |SPRINTF_RETURNS_STRLEN
                                           |KILL_BY_SIGPRC
                                           |(?:PERL|USE|HAS)_\w+)\b/x;
}
close(CFG);

# perl.h logic duplication begins

if ($define{USE_ITHREADS}) {
    if (!$define{MULTIPLICITY}) {
        $define{MULTIPLICITY} = 1;
    }
}

$define{PERL_IMPLICIT_CONTEXT} ||=
    $define{USE_ITHREADS} ||
    $define{MULTIPLICITY} ;

if ($define{USE_ITHREADS} && $ARGS{PLATFORM} ne 'win32' && $^O ne 'darwin') {
    $define{USE_REENTRANT_API} = 1;
}

# perl.h logic duplication ends

print STDERR "Defines: (" . join(' ', sort keys %define) . ")\n"
     unless $ARGS{PLATFORM} eq 'test';

my $sym_ord = 0;
my %ordinal;

if ($ARGS{PLATFORM} eq 'os2') {
    if (open my $fh, '<', 'perl5.def') {
      while (<$fh>) {
	last if /^\s*EXPORTS\b/;
      }
      while (<$fh>) {
	$ordinal{$1} = $2 if /^\s*"(\w+)"\s*(?:=\s*"\w+"\s*)?\@(\d+)\s*$/;
	# This allows skipping ordinals which were used in older versions
	$sym_ord = $1 if /^\s*;\s*LAST_ORDINAL\s*=\s*(\d+)\s*$/;
      }
      $sym_ord < $_ and $sym_ord = $_ for values %ordinal; # Take the max
    }
}

my %skip;
# All platforms export boot_DynaLoader unconditionally.
my %export = ( boot_DynaLoader => 1 );

sub try_symbols {
    foreach my $symbol (@_) {
	++$export{$symbol} unless exists $skip{$symbol};
    }
}

sub readvar {
    # $hash is the hash that we're adding to. For one of our callers, it will
    # actually be the skip hash but that doesn't affect the intent of what
    # we're doing, as in that case we skip adding something to the skip hash
    # for the second time.

    my $file = $ARGS{TARG_DIR} . shift;
    my $hash = shift;
    my $proc = shift;
    open my $vars, '<', $file or die die "Cannot open $file: $!\n";

    while (<$vars>) {
	# All symbols have a Perl_ prefix because that's what embed.h sticks
	# in front of them.  The A?I?S?C? is strictly speaking wrong.
	next unless /\bPERLVAR(A?I?S?C?)\(([IGT]),\s*(\w+)/;

	my $var = "PL_$3";
	my $symbol = $proc ? &$proc($1,$2,$3) : $var;
	++$hash->{$symbol} unless exists $skip{$var};
    }
}

if ($ARGS{PLATFORM} ne 'os2') {
    ++$skip{$_} foreach qw(
		     PL_cryptseen
		     PL_opsave
		     Perl_GetVars
		     Perl_dump_fds
		     Perl_my_bcopy
		     Perl_my_bzero
		     Perl_my_chsize
		     Perl_my_htonl
		     Perl_my_memcmp
		     Perl_my_memset
		     Perl_my_ntohl
		     Perl_my_swap
			 );
    if ($ARGS{PLATFORM} eq 'vms') {
	++$skip{PL_statusvalue_posix};
        # This is a wrapper if we have symlink, not a replacement
        # if we don't.
        ++$skip{Perl_my_symlink} unless $Config{d_symlink};
    } else {
	++$skip{PL_statusvalue_vms};
	if ($ARGS{PLATFORM} ne 'aix') {
	    ++$skip{$_} foreach qw(
				PL_DBcv
				PL_generation
				PL_lastgotoprobe
				PL_modcount
				PL_timesbuf
				main
				 );
	}
    }
}

if ($ARGS{PLATFORM} ne 'vms') {
    # VMS does its own thing for these symbols.
    ++$skip{$_} foreach qw(
			PL_sig_handlers_initted
			PL_sig_ignoring
			PL_sig_defaulting
			 );
    if ($ARGS{PLATFORM} ne 'win32') {
	++$skip{$_} foreach qw(
			    Perl_do_spawn
			    Perl_do_spawn_nowait
			    Perl_do_aspawn
			     );
    }
}

if ($ARGS{PLATFORM} ne 'win32') {
    ++$skip{$_} foreach qw(
		    Perl_my_setlocale
			 );
}

unless ($define{UNLINK_ALL_VERSIONS}) {
    ++$skip{Perl_unlnk};
}

unless ($define{'DEBUGGING'}) {
    ++$skip{$_} foreach qw(
		    Perl_debop
		    Perl_debprofdump
		    Perl_debstack
		    Perl_debstackptrs
		    Perl_pad_sv
		    Perl_pad_setsv
		    Perl_hv_assert
		    PL_watchaddr
		    PL_watchok
		    PL_watch_pvx
			 );
}

if ($define{'PERL_IMPLICIT_SYS'}) {
    ++$skip{$_} foreach qw(
		    Perl_my_popen
		    Perl_my_pclose
			 );
    ++$export{$_} foreach qw(perl_get_host_info perl_alloc_override);
    ++$export{perl_clone_host} if $define{USE_ITHREADS};
}
else {
    ++$skip{$_} foreach qw(
		    PL_Mem
		    PL_MemShared
		    PL_MemParse
		    PL_Env
		    PL_StdIO
		    PL_LIO
		    PL_Dir
		    PL_Sock
		    PL_Proc
		    perl_alloc_using
		    perl_clone_using
			 );
}

unless ($define{'PERL_OLD_COPY_ON_WRITE'}
     || $define{'PERL_NEW_COPY_ON_WRITE'}) {
    ++$skip{Perl_sv_setsv_cow};
}

unless ($define{PERL_SAWAMPERSAND}) {
    ++$skip{PL_sawampersand};
}

unless ($define{'USE_REENTRANT_API'}) {
    ++$skip{PL_reentrant_buffer};
}

if ($define{'MYMALLOC'}) {
    try_symbols(qw(
		    Perl_dump_mstats
		    Perl_get_mstats
		    Perl_strdup
		    Perl_putenv
		    MallocCfg_ptr
		    MallocCfgP_ptr
		    ));
    unless ($define{USE_ITHREADS}) {
	++$skip{PL_malloc_mutex}
    }
}
else {
    ++$skip{$_} foreach qw(
		    PL_malloc_mutex
		    Perl_dump_mstats
		    Perl_get_mstats
		    MallocCfg_ptr
		    MallocCfgP_ptr
			 );
}

if ($define{'PERL_USE_SAFE_PUTENV'}) {
    ++$skip{PL_use_safe_putenv};
}

unless ($define{'USE_ITHREADS'}) {
    ++$skip{PL_thr_key};
}

# USE_5005THREADS symbols. Kept as reference for easier removal
++$skip{$_} foreach qw(
		    PL_sv_mutex
		    PL_strtab_mutex
		    PL_svref_mutex
		    PL_cred_mutex
		    PL_eval_mutex
		    PL_fdpid_mutex
		    PL_sv_lock_mutex
		    PL_eval_cond
		    PL_eval_owner
		    PL_threads_mutex
		    PL_nthreads
		    PL_nthreads_cond
		    PL_threadnum
		    PL_threadsv_names
		    PL_thrsv
		    PL_vtbl_mutex
		    Perl_condpair_magic
		    Perl_new_struct_thread
		    Perl_per_thread_magicals
		    Perl_thread_create
		    Perl_find_threadsv
		    Perl_unlock_condpair
		    Perl_magic_mutexfree
		    Perl_sv_lock
		     );

unless ($define{'USE_ITHREADS'}) {
    ++$skip{$_} foreach qw(
		    PL_check_mutex
		    PL_op_mutex
		    PL_regex_pad
		    PL_regex_padav
		    PL_dollarzero_mutex
		    PL_hints_mutex
		    PL_my_ctx_mutex
		    PL_perlio_mutex
		    PL_stashpad
		    PL_stashpadix
		    PL_stashpadmax
		    Perl_alloccopstash
		    Perl_allocfilegv
		    Perl_clone_params_del
		    Perl_clone_params_new
		    Perl_parser_dup
		    Perl_dirp_dup
		    Perl_cx_dup
		    Perl_si_dup
		    Perl_any_dup
		    Perl_ss_dup
		    Perl_fp_dup
		    Perl_gp_dup
		    Perl_he_dup
		    Perl_mg_dup
		    Perl_re_dup_guts
		    Perl_sv_dup
		    Perl_sv_dup_inc
		    Perl_rvpv_dup
		    Perl_hek_dup
		    Perl_sys_intern_dup
		    perl_clone
		    perl_clone_using
		    Perl_stashpv_hvname_match
		    Perl_regdupe_internal
		    Perl_newPADOP
			 );
}

unless ($define{'PERL_IMPLICIT_CONTEXT'}) {
    ++$skip{$_} foreach qw(
		    PL_my_cxt_index
		    PL_my_cxt_list
		    PL_my_cxt_size
		    PL_my_cxt_keys
		    Perl_croak_nocontext
		    Perl_die_nocontext
		    Perl_deb_nocontext
		    Perl_form_nocontext
		    Perl_load_module_nocontext
		    Perl_mess_nocontext
		    Perl_warn_nocontext
		    Perl_warner_nocontext
		    Perl_newSVpvf_nocontext
		    Perl_sv_catpvf_nocontext
		    Perl_sv_setpvf_nocontext
		    Perl_sv_catpvf_mg_nocontext
		    Perl_sv_setpvf_mg_nocontext
		    Perl_my_cxt_init
		    Perl_my_cxt_index
			 );
}

unless ($define{'PERL_NEED_APPCTX'}) {
    ++$skip{PL_appctx};
}

unless ($define{'PERL_NEED_TIMESBASE'}) {
    ++$skip{PL_timesbase};
}

unless ($define{'DEBUG_LEAKING_SCALARS'}) {
    ++$skip{PL_sv_serial};
}

unless ($define{'DEBUG_LEAKING_SCALARS_FORK_DUMP'}) {
    ++$skip{PL_dumper_fd};
}

unless ($define{'PERL_DONT_CREATE_GVSV'}) {
    ++$skip{Perl_gv_SVadd};
}

if ($define{'SPRINTF_RETURNS_STRLEN'}) {
    ++$skip{Perl_my_sprintf};
}

unless ($define{'PERL_USES_PL_PIDSTATUS'}) {
    ++$skip{PL_pidstatus};
}

unless ($define{'PERL_TRACK_MEMPOOL'}) {
    ++$skip{PL_memory_debug_header};
}

unless ($define{PERL_MAD}) {
    ++$skip{$_} foreach qw(
		    PL_madskills
		    PL_xmlfp
			 );
}

unless ($define{'MULTIPLICITY'}) {
    ++$skip{$_} foreach qw(
		    PL_interp_size
		    PL_interp_size_5_18_0
			 );
}

unless ($define{'PERL_GLOBAL_STRUCT'}) {
    ++$skip{PL_global_struct_size};
}

unless ($define{'PERL_GLOBAL_STRUCT_PRIVATE'}) {
    ++$skip{$_} foreach qw(
		    PL_my_cxt_keys
		    Perl_my_cxt_index
			 );
}

unless ($define{HAS_MMAP}) {
    ++$skip{PL_mmap_page_size};
}

if ($define{HAS_SIGACTION}) {
    ++$skip{PL_sig_trapped};

    if ($ARGS{PLATFORM} eq 'vms') {
        # FAKE_PERSISTENT_SIGNAL_HANDLERS defined as !defined(HAS_SIGACTION)
        ++$skip{PL_sig_ignoring};
        ++$skip{PL_sig_handlers_initted} unless $define{KILL_BY_SIGPRC};
    }
}

if ($ARGS{PLATFORM} eq 'vms' && !$define{KILL_BY_SIGPRC}) {
    # FAKE_DEFAULT_SIGNAL_HANDLERS defined as KILL_BY_SIGPRC
    ++$skip{Perl_csighandler_init};
    ++$skip{Perl_my_kill};
    ++$skip{Perl_sig_to_vmscondition};
    ++$skip{PL_sig_defaulting};
    ++$skip{PL_sig_handlers_initted} unless !$define{HAS_SIGACTION};
}

unless ($define{USE_LOCALE_COLLATE}) {
    ++$skip{$_} foreach qw(
		    PL_collation_ix
		    PL_collation_name
		    PL_collation_standard
		    PL_collxfrm_base
		    PL_collxfrm_mult
		    Perl_sv_collxfrm
		    Perl_sv_collxfrm_flags
			 );
}

unless ($define{USE_LOCALE_NUMERIC}) {
    ++$skip{$_} foreach qw(
		    PL_numeric_local
		    PL_numeric_name
		    PL_numeric_radix_sv
		    PL_numeric_standard
			 );
}

unless ($define{HAVE_INTERP_INTERN}) {
    ++$skip{$_} foreach qw(
		    Perl_sys_intern_clear
		    Perl_sys_intern_dup
		    Perl_sys_intern_init
		    PL_sys_intern
			 );
}

if ($define{HAS_SIGNBIT}) {
    ++$skip{Perl_signbit};
}

if ($define{'PERL_GLOBAL_STRUCT'}) {
    readvar('perlvars.h', \%skip);
    # This seems like the least ugly way to cope with the fact that PL_sh_path
    # is mentioned in perlvar.h and globvar.sym, and always exported.
    delete $skip{PL_sh_path};
    ++$export{Perl_GetVars};
    try_symbols(qw(PL_Vars PL_VarsPtr))
      unless $ARGS{CCTYPE} eq 'GCC' || $define{PERL_GLOBAL_STRUCT_PRIVATE};
} else {
    ++$skip{$_} foreach qw(Perl_init_global_struct Perl_free_global_struct);
}

++$skip{PL_op_exec_cnt}
    unless $define{PERL_TRACE_OPS};

# functions from *.sym files

my @syms = qw(globvar.sym);

# Symbols that are the public face of the PerlIO layers implementation
# These are in _addition to_ the public face of the abstraction
# and need to be exported to allow XS modules to implement layers
my @layer_syms = qw(
		    PerlIOBase_binmode
		    PerlIOBase_clearerr
		    PerlIOBase_close
		    PerlIOBase_dup
		    PerlIOBase_eof
		    PerlIOBase_error
		    PerlIOBase_fileno
		    PerlIOBase_open
		    PerlIOBase_noop_fail
		    PerlIOBase_noop_ok
		    PerlIOBase_popped
		    PerlIOBase_pushed
		    PerlIOBase_read
		    PerlIOBase_setlinebuf
		    PerlIOBase_unread
		    PerlIOBuf_bufsiz
		    PerlIOBuf_close
		    PerlIOBuf_dup
		    PerlIOBuf_fill
		    PerlIOBuf_flush
		    PerlIOBuf_get_base
		    PerlIOBuf_get_cnt
		    PerlIOBuf_get_ptr
		    PerlIOBuf_open
		    PerlIOBuf_popped
		    PerlIOBuf_pushed
		    PerlIOBuf_read
		    PerlIOBuf_seek
		    PerlIOBuf_set_ptrcnt
		    PerlIOBuf_tell
		    PerlIOBuf_unread
		    PerlIOBuf_write
		    PerlIO_allocate
		    PerlIO_apply_layera
		    PerlIO_apply_layers
		    PerlIO_arg_fetch
		    PerlIO_debug
		    PerlIO_define_layer
		    PerlIO_find_layer
		    PerlIO_isutf8
		    PerlIO_layer_fetch
		    PerlIO_list_alloc
		    PerlIO_list_free
		    PerlIO_modestr
		    PerlIO_parse_layers
		    PerlIO_pending
		    PerlIO_perlio
		    PerlIO_pop
		    PerlIO_push
		    PerlIO_sv_dup
		    Perl_PerlIO_clearerr
		    Perl_PerlIO_close
		    Perl_PerlIO_context_layers
		    Perl_PerlIO_eof
		    Perl_PerlIO_error
		    Perl_PerlIO_fileno
		    Perl_PerlIO_fill
		    Perl_PerlIO_flush
		    Perl_PerlIO_get_base
		    Perl_PerlIO_get_bufsiz
		    Perl_PerlIO_get_cnt
		    Perl_PerlIO_get_ptr
		    Perl_PerlIO_read
		    Perl_PerlIO_seek
		    Perl_PerlIO_set_cnt
		    Perl_PerlIO_set_ptrcnt
		    Perl_PerlIO_setlinebuf
		    Perl_PerlIO_stderr
		    Perl_PerlIO_stdin
		    Perl_PerlIO_stdout
		    Perl_PerlIO_tell
		    Perl_PerlIO_unread
		    Perl_PerlIO_write
);
if ($ARGS{PLATFORM} eq 'netware') {
    push(@layer_syms,'PL_def_layerlist','PL_known_layers','PL_perlio');
}

if ($define{'USE_PERLIO'}) {
    # Export the symbols that make up the PerlIO abstraction, regardless
    # of its implementation - read from a file
    push @syms, 'perlio.sym';

    # PerlIO with layers - export implementation
    try_symbols(@layer_syms, 'perlsio_binmode');
} else {
	# -Uuseperlio
	# Skip the PerlIO layer symbols - although
	# nothing should have exported them anyway.
	++$skip{$_} foreach @layer_syms;
	++$skip{$_} foreach qw(
			perlsio_binmode
			PL_def_layerlist
			PL_known_layers
			PL_perlio
			PL_perlio_debug_fd
			PL_perlio_fd_refcnt
			PL_perlio_fd_refcnt_size
			PL_perlio_mutex
			     );

	# Also do NOT add abstraction symbols from $perlio_sym
	# abstraction is done as #define to stdio
	# Remaining remnants that _may_ be functions are handled below.
}

###############################################################################

# At this point all skip lists should be completed, as we are about to test
# many symbols against them.

{
    my %seen;
    my ($embed) = setup_embed($ARGS{TARG_DIR});

    foreach (@$embed) {
	my ($flags, $retval, $func, @args) = @$_;
	next unless $func;
	if ($flags =~ /[AX]/ && $flags !~ /[xmi]/ || $flags =~ /b/) {
	    # public API, so export

	    # If a function is defined twice, for example before and after
	    # an #else, only export its name once. Important to do this test
	    # within the block, as the *first* definition may have flags which
	    # mean "don't export"
	    next if $seen{$func}++;
	    # Should we also skip adding the Perl_ prefix if $flags =~ /o/ ?
	    $func = "Perl_$func" if ($flags =~ /[pbX]/ && $func !~ /^Perl_/); 
	    ++$export{$func} unless exists $skip{$func};
	}
    }
}

foreach (@syms) {
    my $syms = $ARGS{TARG_DIR} . $_;
    open my $global, '<', $syms or die "failed to open $syms: $!\n";
    # Functions already have a Perl_ prefix
    # Variables need a PL_ prefix
    my $prefix = $syms =~ /var\.sym$/i ? 'PL_' : '';
    while (<$global>) {
	next unless /^([A-Za-z].*)/;
	my $symbol = "$prefix$1";
	++$export{$symbol} unless exists $skip{$symbol};
    }
}

# variables

if ($define{'MULTIPLICITY'} && $define{PERL_GLOBAL_STRUCT}) {
    readvar('perlvars.h', \%export, sub { "Perl_" . $_[1] . $_[2] . "_ptr" });
    # XXX AIX seems to want the perlvars.h symbols, for some reason
    if ($ARGS{PLATFORM} eq 'aix' or $ARGS{PLATFORM} eq 'os2') {	# OS/2 needs PL_thr_key
	readvar('perlvars.h', \%export);
    }
}
else {
    unless ($define{'PERL_GLOBAL_STRUCT'}) {
	readvar('perlvars.h', \%export);
    }
    unless ($define{MULTIPLICITY}) {
	readvar('intrpvar.h', \%export);
    }
}

# Oddities from PerlIO
# All have alternate implementations in perlio.c, so always exist.
# Should they be considered to be part of the API?
try_symbols(qw(
		    PerlIO_binmode
		    PerlIO_getpos
		    PerlIO_init
		    PerlIO_setpos
		    PerlIO_tmpfile
	     ));

if ($ARGS{PLATFORM} eq 'win32') {
    try_symbols(qw(
				 win32_free_childdir
				 win32_free_childenv
				 win32_get_childdir
				 win32_get_childenv
				 win32_spawnvp
		 ));
}

if ($ARGS{PLATFORM} eq 'wince') {
    ++$skip{'win32_isatty'}; # commit 4342f4d6df is win32-only
}

if ($ARGS{PLATFORM} =~ /^win(?:32|ce)$/) {
    try_symbols(qw(
			    Perl_init_os_extras
			    Perl_thread_create
			    Perl_win32_init
			    Perl_win32_term
			    RunPerl
			    win32_async_check
			    win32_errno
			    win32_environ
			    win32_abort
			    win32_fstat
			    win32_stat
			    win32_pipe
			    win32_popen
			    win32_pclose
			    win32_rename
			    win32_setmode
			    win32_chsize
			    win32_lseek
			    win32_tell
			    win32_dup
			    win32_dup2
			    win32_open
			    win32_close
			    win32_eof
			    win32_isatty
			    win32_read
			    win32_write
			    win32_mkdir
			    win32_rmdir
			    win32_chdir
			    win32_flock
			    win32_execv
			    win32_execvp
			    win32_htons
			    win32_ntohs
			    win32_htonl
			    win32_ntohl
			    win32_inet_addr
			    win32_inet_ntoa
			    win32_socket
			    win32_bind
			    win32_listen
			    win32_accept
			    win32_connect
			    win32_send
			    win32_sendto
			    win32_recv
			    win32_recvfrom
			    win32_shutdown
			    win32_closesocket
			    win32_ioctlsocket
			    win32_setsockopt
			    win32_getsockopt
			    win32_getpeername
			    win32_getsockname
			    win32_gethostname
			    win32_gethostbyname
			    win32_gethostbyaddr
			    win32_getprotobyname
			    win32_getprotobynumber
			    win32_getservbyname
			    win32_getservbyport
			    win32_select
			    win32_endhostent
			    win32_endnetent
			    win32_endprotoent
			    win32_endservent
			    win32_getnetent
			    win32_getnetbyname
			    win32_getnetbyaddr
			    win32_getprotoent
			    win32_getservent
			    win32_sethostent
			    win32_setnetent
			    win32_setprotoent
			    win32_setservent
			    win32_getenv
			    win32_putenv
			    win32_perror
			    win32_malloc
			    win32_calloc
			    win32_realloc
			    win32_free
			    win32_sleep
			    win32_times
			    win32_access
			    win32_alarm
			    win32_chmod
			    win32_open_osfhandle
			    win32_get_osfhandle
			    win32_ioctl
			    win32_link
			    win32_unlink
			    win32_utime
			    win32_gettimeofday
			    win32_uname
			    win32_wait
			    win32_waitpid
			    win32_kill
			    win32_str_os_error
			    win32_opendir
			    win32_readdir
			    win32_telldir
			    win32_seekdir
			    win32_rewinddir
			    win32_closedir
			    win32_longpath
			    win32_ansipath
			    win32_os_id
			    win32_getpid
			    win32_crypt
			    win32_dynaload
			    win32_clearenv
			    win32_stdin
			    win32_stdout
			    win32_stderr
			    win32_ferror
			    win32_feof
			    win32_strerror
			    win32_fprintf
			    win32_printf
			    win32_vfprintf
			    win32_vprintf
			    win32_fread
			    win32_fwrite
			    win32_fopen
			    win32_fdopen
			    win32_freopen
			    win32_fclose
			    win32_fputs
			    win32_fputc
			    win32_ungetc
			    win32_getc
			    win32_fileno
			    win32_clearerr
			    win32_fflush
			    win32_ftell
			    win32_fseek
			    win32_fgetpos
			    win32_fsetpos
			    win32_rewind
			    win32_tmpfile
			    win32_setbuf
			    win32_setvbuf
			    win32_flushall
			    win32_fcloseall
			    win32_fgets
			    win32_gets
			    win32_fgetc
			    win32_putc
			    win32_puts
			    win32_getchar
			    win32_putchar
		 ));
}
elsif ($ARGS{PLATFORM} eq 'vms') {
    try_symbols(qw(
		      Perl_cando
		      Perl_cando_by_name
		      Perl_closedir
		      Perl_csighandler_init
		      Perl_do_rmdir
		      Perl_fileify_dirspec
		      Perl_fileify_dirspec_ts
		      Perl_fileify_dirspec_utf8
		      Perl_fileify_dirspec_utf8_ts
		      Perl_flex_fstat
		      Perl_flex_lstat
		      Perl_flex_stat
		      Perl_kill_file
		      Perl_my_chdir
		      Perl_my_chmod
		      Perl_my_crypt
		      Perl_my_endpwent
		      Perl_my_fclose
		      Perl_my_fdopen
		      Perl_my_fgetname
		      Perl_my_flush
		      Perl_my_fwrite
		      Perl_my_gconvert
		      Perl_my_getenv
		      Perl_my_getenv_len
		      Perl_my_getlogin
		      Perl_my_getpwnam
		      Perl_my_getpwuid
		      Perl_my_gmtime
		      Perl_my_kill
		      Perl_my_localtime
		      Perl_my_mkdir
		      Perl_my_sigaction
		      Perl_my_symlink
		      Perl_my_time
		      Perl_my_tmpfile
		      Perl_my_trnlnm
		      Perl_my_utime
		      Perl_my_waitpid
		      Perl_opendir
		      Perl_pathify_dirspec
		      Perl_pathify_dirspec_ts
		      Perl_pathify_dirspec_utf8
		      Perl_pathify_dirspec_utf8_ts
		      Perl_readdir
		      Perl_readdir_r
		      Perl_rename
		      Perl_rmscopy
		      Perl_rmsexpand
		      Perl_rmsexpand_ts
		      Perl_rmsexpand_utf8
		      Perl_rmsexpand_utf8_ts
		      Perl_seekdir
		      Perl_sig_to_vmscondition
		      Perl_telldir
		      Perl_tounixpath
		      Perl_tounixpath_ts
		      Perl_tounixpath_utf8
		      Perl_tounixpath_utf8_ts
		      Perl_tounixspec
		      Perl_tounixspec_ts
		      Perl_tounixspec_utf8
		      Perl_tounixspec_utf8_ts
		      Perl_tovmspath
		      Perl_tovmspath_ts
		      Perl_tovmspath_utf8
		      Perl_tovmspath_utf8_ts
		      Perl_tovmsspec
		      Perl_tovmsspec_ts
		      Perl_tovmsspec_utf8
		      Perl_tovmsspec_utf8_ts
		      Perl_trim_unixpath
		      Perl_vms_case_tolerant
		      Perl_vms_do_aexec
		      Perl_vms_do_exec
		      Perl_vms_image_init
		      Perl_vms_realpath
		      Perl_vmssetenv
		      Perl_vmssetuserlnm
		      Perl_vmstrnenv
		      PerlIO_openn
		 ));
}
elsif ($ARGS{PLATFORM} eq 'os2') {
    try_symbols(qw(
		      ctermid
		      get_sysinfo
		      Perl_OS2_init
		      Perl_OS2_init3
		      Perl_OS2_term
		      OS2_Perl_data
		      dlopen
		      dlsym
		      dlerror
		      dlclose
		      dup2
		      dup
		      my_tmpfile
		      my_tmpnam
		      my_flock
		      my_rmdir
		      my_mkdir
		      my_getpwuid
		      my_getpwnam
		      my_getpwent
		      my_setpwent
		      my_endpwent
		      fork_with_resources
		      croak_with_os2error
		      setgrent
		      endgrent
		      getgrent
		      malloc_mutex
		      threads_mutex
		      nthreads
		      nthreads_cond
		      os2_cond_wait
		      os2_stat
		      os2_execname
		      async_mssleep
		      msCounter
		      InfoTable
		      pthread_join
		      pthread_create
		      pthread_detach
		      XS_Cwd_change_drive
		      XS_Cwd_current_drive
		      XS_Cwd_extLibpath
		      XS_Cwd_extLibpath_set
		      XS_Cwd_sys_abspath
		      XS_Cwd_sys_chdir
		      XS_Cwd_sys_cwd
		      XS_Cwd_sys_is_absolute
		      XS_Cwd_sys_is_relative
		      XS_Cwd_sys_is_rooted
		      XS_DynaLoader_mod2fname
		      XS_File__Copy_syscopy
		      Perl_Register_MQ
		      Perl_Deregister_MQ
		      Perl_Serve_Messages
		      Perl_Process_Messages
		      init_PMWIN_entries
		      PMWIN_entries
		      Perl_hab_GET
		      loadByOrdinal
		      pExtFCN
		      os2error
		      ResetWinError
		      CroakWinError
		      PL_do_undump
		 ));
}
elsif ($ARGS{PLATFORM} eq 'netware') {
    try_symbols(qw(
			Perl_init_os_extras
			Perl_thread_create
			Perl_nw5_init
			RunPerl
			AllocStdPerl
			FreeStdPerl
			do_spawn2
			do_aspawn
			nw_uname
			nw_stdin
			nw_stdout
			nw_stderr
			nw_feof
			nw_ferror
			nw_fopen
			nw_fclose
			nw_clearerr
			nw_getc
			nw_fgets
			nw_fputc
			nw_fputs
			nw_fflush
			nw_ungetc
			nw_fileno
			nw_fdopen
			nw_freopen
			nw_fread
			nw_fwrite
			nw_setbuf
			nw_setvbuf
			nw_vfprintf
			nw_ftell
			nw_fseek
			nw_rewind
			nw_tmpfile
			nw_fgetpos
			nw_fsetpos
			nw_dup
			nw_access
			nw_chmod
			nw_chsize
			nw_close
			nw_dup2
			nw_flock
			nw_isatty
			nw_link
			nw_lseek
			nw_stat
			nw_mktemp
			nw_open
			nw_read
			nw_rename
			nw_setmode
			nw_unlink
			nw_utime
			nw_write
			nw_chdir
			nw_rmdir
			nw_closedir
			nw_opendir
			nw_readdir
			nw_rewinddir
			nw_seekdir
			nw_telldir
			nw_htonl
			nw_htons
			nw_ntohl
			nw_ntohs
			nw_accept
			nw_bind
			nw_connect
			nw_endhostent
			nw_endnetent
			nw_endprotoent
			nw_endservent
			nw_gethostbyaddr
			nw_gethostbyname
			nw_gethostent
			nw_gethostname
			nw_getnetbyaddr
			nw_getnetbyname
			nw_getnetent
			nw_getpeername
			nw_getprotobyname
			nw_getprotobynumber
			nw_getprotoent
			nw_getservbyname
			nw_getservbyport
			nw_getservent
			nw_getsockname
			nw_getsockopt
			nw_inet_addr
			nw_listen
			nw_socket
			nw_recv
			nw_recvfrom
			nw_select
			nw_send
			nw_sendto
			nw_sethostent
			nw_setnetent
			nw_setprotoent
			nw_setservent
			nw_setsockopt
			nw_inet_ntoa
			nw_shutdown
			nw_crypt
			nw_execvp
			nw_kill
			nw_Popen
			nw_Pclose
			nw_Pipe
			nw_times
			nw_waitpid
			nw_getpid
			nw_spawnvp
			nw_os_id
			nw_open_osfhandle
			nw_get_osfhandle
			nw_abort
			nw_sleep
			nw_wait
			nw_dynaload
			nw_strerror
			fnFpSetMode
			fnInsertHashListAddrs
			fnGetHashListAddrs
			Perl_deb
			Perl_sv_setsv
			Perl_sv_catsv
			Perl_sv_catpvn
			Perl_sv_2pv
			nw_freeenviron
			Remove_Thread_Ctx
		 ));
}

# When added this code was only run for Win32 and WinCE
# Currently only Win32 links static extensions into the shared library.
# The WinCE makefile doesn't appear to support static extensions, so this code
# can't have any effect there.
# The NetWare Makefile doesn't support static extensions (and hardcodes the
# list of dynamic extensions, and the rules to build them)
# For *nix (and presumably OS/2) with a shared libperl, Makefile.SH compiles
# static extensions with -fPIC, but links them to perl, not libperl.so
# The VMS build scripts don't yet implement static extensions at all.

if ($ARGS{PLATFORM} =~ /^win(?:32|ce)$/) {
    # records of type boot_module for statically linked modules (except Dynaloader)
    my $static_ext = $Config{static_ext} // "";
    $static_ext =~ s/\//__/g;
    $static_ext =~ s/\bDynaLoader\b//;
    try_symbols(map {"boot_$_"} grep {/\S/} split /\s+/, $static_ext);
    try_symbols("init_Win32CORE") if $static_ext =~ /\bWin32CORE\b/;
}

if ($ARGS{PLATFORM} eq 'os2') {
    my (%mapped, @missing);
    open MAP, 'miniperl.map' or die 'Cannot read miniperl.map';
    /^\s*[\da-f:]+\s+(\w+)/i and $mapped{$1}++ foreach <MAP>;
    close MAP or die 'Cannot close miniperl.map';

    @missing = grep { !exists $mapped{$_} }
		    keys %export;
    @missing = grep { !exists $exportperlmalloc{$_} } @missing;
    delete $export{$_} foreach @missing;
}

###############################################################################

# Now all symbols should be defined because next we are going to output them.

# Start with platform specific headers:

if ($ARGS{PLATFORM} =~ /^win(?:32|ce)$/) {
    my $dll = $define{PERL_DLL} ? $define{PERL_DLL} =~ s/\.dll$//ir
	: "perl$Config{api_revision}$Config{api_version}";
    print "LIBRARY $dll\n";
    # The DESCRIPTION module definition file statement is not supported
    # by VC7 onwards.
    if ($ARGS{CCTYPE} =~ /^(?:MSVC60|GCC)$/) {
	print "DESCRIPTION 'Perl interpreter'\n";
    }
    print "EXPORTS\n";
}
elsif ($ARGS{PLATFORM} eq 'os2') {
    (my $v = $]) =~ s/(\d\.\d\d\d)(\d\d)$/$1_$2/;
    $v .= '-thread' if $Config{archname} =~ /-thread/;
    (my $dll = $define{PERL_DLL}) =~ s/\.dll$//i;
    $v .= "\@$Config{perl_patchlevel}" if $Config{perl_patchlevel};
    my $d = "DESCRIPTION '\@#perl5-porters\@perl.org:$v#\@ Perl interpreter, configured as $Config{config_args}'";
    $d = substr($d, 0, 249) . "...'" if length $d > 253;
    print <<"---EOP---";
LIBRARY '$dll' INITINSTANCE TERMINSTANCE
$d
STACKSIZE 32768
CODE LOADONCALL
DATA LOADONCALL NONSHARED MULTIPLE
EXPORTS
---EOP---
}
elsif ($ARGS{PLATFORM} eq 'aix') {
    my $OSVER = `uname -v`;
    chop $OSVER;
    my $OSREL = `uname -r`;
    chop $OSREL;
    if ($OSVER > 4 || ($OSVER == 4 && $OSREL >= 3)) {
	print "#! ..\n";
    } else {
	print "#!\n";
    }
}
elsif ($ARGS{PLATFORM} eq 'netware') {
	if ($ARGS{FILETYPE} eq 'def') {
	print "LIBRARY perl$Config{api_revision}$Config{api_version}\n";
	print "DESCRIPTION 'Perl interpreter for NetWare'\n";
	print "EXPORTS\n";
	}
}

# Then the symbols

my @symbols = $fold ? sort {lc $a cmp lc $b} keys %export : sort keys %export;
foreach my $symbol (@symbols) {
    if ($ARGS{PLATFORM} =~ /^win(?:32|ce)$/) {
	print "\t$symbol\n";
    }
    elsif ($ARGS{PLATFORM} eq 'os2') {
	printf qq(    %-31s \@%s\n),
	  qq("$symbol"), $ordinal{$symbol} || ++$sym_ord;
	printf qq(    %-31s \@%s\n),
	  qq("$exportperlmalloc{$symbol}" = "$symbol"),
	  $ordinal{$exportperlmalloc{$symbol}} || ++$sym_ord
	  if $exportperlmalloc and exists $exportperlmalloc{$symbol};
    }
    elsif ($ARGS{PLATFORM} eq 'netware') {
	print "\t$symbol,\n";
    } else {
	print "$symbol\n";
    }
}

# Then platform specific footers.

if ($ARGS{PLATFORM} eq 'os2') {
    print <<EOP;
    dll_perlmain=main
    fill_extLibpath
    dir_subst
    Perl_OS2_handler_install

; LAST_ORDINAL=$sym_ord
EOP
}

1;
