
# creates a C API file from proto.h
# takes one argument, the path to lib/CORE directory.
# creates 2 files: "perlCAPI.cpp" and "perlCAPI.h".

#use Config;

my $hdrfile = "$ARGV[0]\\perlCAPI.h";
my $infile = '..\\proto.h';
my $embedfile = '..\\embed.h';
my $separateObj = 0;

my %skip_list;
my %embed;

sub readembed(\%$) {
    my ($syms, $file) = @_;
    my ($line, @words);
    %$syms = ();
    local (*FILE, $_);
    open(FILE, "< $file")
	or die "$0: Can't open $file: $!\n";
    while ($line = <FILE>) {
	chop($line);
	if ($line =~ /^#define\s+\w+/) {
	    $line =~ s/^#define\s+//;
	    @words = split ' ', $line;
#	    print "$words[0]\t$words[1]\n";
	    $$syms{$words[0]} = $words[1];
	}
    }
    close(FILE);
}

readembed %embed, $embedfile;

sub skip_these {
    my $list = shift;
    foreach my $symbol (@$list) {
	$skip_list{$symbol} = 1;
    }
}

skip_these [qw(
cando
cast_ulong
my_chsize
condpair_magic
deb
deb_growlevel
debprofdump
debop
debstack
debstackptrs
dump_fds
dump_mstats
fprintf
find_threadsv
magic_mutexfree
my_memcmp
my_memset
my_pclose
my_popen
my_swap
my_htonl
my_ntohl
new_struct_thread
same_dirent
unlnk
unlock_condpair
safexmalloc
safexcalloc
safexrealloc
safexfree
Perl_GetVars
malloced_size
)];



if (!open(INFILE, "<$infile")) {
    print "open of $infile failed: $!\n";
    return 1;
}

if (!open(OUTFILE, ">perlCAPI.cpp")) {
    print "open of perlCAPI.cpp failed: $!\n";
    return 1;
}

print OUTFILE <<ENDCODE;
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
  
#define DESTRUCTORFUNC (void (*)(void*))
  
ENDCODE

print OUTFILE "#ifdef SetCPerlObj_defined\n" unless ($separateObj == 0);

print OUTFILE <<ENDCODE;
EXTERN_C void SetCPerlObj(void *pP)
{
    pPerl = (CPerlObj*)pP;
}
  
ENDCODE

print OUTFILE "#endif\n" unless ($separateObj == 0); 

while () {
    last unless defined ($_ = <INFILE>);
    if (/^VIRTUAL\s/) {
        while (!/;$/) {
            chomp;
            $_ .= <INFILE>;
        }
        $_ =~ s/^VIRTUAL\s*//;
        $_ =~ s/\s*__attribute__.*$/;/;
        if ( /(.*)\s([A-z_]*[0-9A-z_]+\s)_\(\((.*)\)\);/ ||
             /(.*)\*([A-z_]*[0-9A-z_]+\s)_\(\((.*)\)\);/ ) {
            $type = $1;
            $name = $2;
            $args = $3;
 
            $name =~ s/\s*$//;
            $type =~ s/\s*$//;
	    next if (defined $skip_list{$name});

	    if($args eq "ARGSproto") {
		$args = "void";
	    }

            $return = ($type eq "void" or $type eq "Free_t") ? "\t" : "\treturn";

	    if(defined $embed{$name}) {
		$funcName = $embed{$name};
	    } else {
		$funcName = $name;
	    }

            @args = split(',', $args);
            if ($args[$#args] =~ /\s*\.\.\.\s*/) {
                if(($name eq "croak") or ($name eq "deb") or ($name eq "die")
		        or ($name eq "form") or ($name eq "warn")) {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $funcName ($args)
{
    char *pstr;
    char *pmsg;
    va_list args;
    va_start(args, $arg);
    pmsg = pPerl->Perl_mess($arg, &args);
    New(0, pstr, strlen(pmsg)+1, char);
    strcpy(pstr, pmsg);
$return pPerl->Perl_$name(pstr);
    va_end(args);
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                }
                elsif($name eq "newSVpvf") {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $funcName ($args)
{
    SV *sv;
    va_list args;
    va_start(args, $arg);
    sv = pPerl->Perl_newSV(0);
    pPerl->Perl_sv_vcatpvfn(sv, $arg, strlen($arg), &args, NULL, 0, NULL);
    va_end(args);
    return sv;
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                }
                elsif($name eq "sv_catpvf") {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg0 = $1;
                    $args[1] =~ /(\w+)\W*$/; 
                    $arg1 = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $funcName ($args)
{
    va_list args;
    va_start(args, $arg1);
    pPerl->Perl_sv_vcatpvfn($arg0, $arg1, strlen($arg1), &args, NULL, 0, NULL);
    va_end(args);
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                }
                elsif($name eq "sv_catpvf_mg") {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg0 = $1;
                    $args[1] =~ /(\w+)\W*$/; 
                    $arg1 = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
#ifndef mg_set
#define mg_set pPerl->Perl_mg_set
#endif
EXTERN_C $type $funcName ($args)
{
    va_list args;
    va_start(args, $arg1);
    pPerl->Perl_sv_vcatpvfn($arg0, $arg1, strlen($arg1), &args, NULL, 0, NULL);
    va_end(args);
    SvSETMAGIC(sv);
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                }
                elsif($name eq "sv_setpvf") {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg0 = $1;
                    $args[1] =~ /(\w+)\W*$/; 
                    $arg1 = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $funcName ($args)
{
    va_list args;
    va_start(args, $arg1);
    pPerl->Perl_sv_vsetpvfn($arg0, $arg1, strlen($arg1), &args, NULL, 0, NULL);
    va_end(args);
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                }
                elsif($name eq "sv_setpvf_mg") {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg0 = $1;
                    $args[1] =~ /(\w+)\W*$/; 
                    $arg1 = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
#ifndef mg_set
#define mg_set pPerl->Perl_mg_set
#endif
EXTERN_C $type $funcName ($args)
{
    va_list args;
    va_start(args, $arg1);
    pPerl->Perl_sv_vsetpvfn($arg0, $arg1, strlen($arg1), &args, NULL, 0, NULL);
    va_end(args);
    SvSETMAGIC(sv);
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                }
                elsif($name eq "fprintf") {
                    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
                    $args[0] =~ /(\w+)\W*$/; 
                    $arg0 = $1;
                    $args[1] =~ /(\w+)\W*$/; 
                    $arg1 = $1;
                    print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $name ($args)
{
    int nRet;
    va_list args;
    va_start(args, $arg1);
    nRet = PerlIO_vprintf($arg0, $arg1, args);
    va_end(args);
    return nRet;
}
ENDCODE
                    print OUTFILE "#endif\n" unless ($separateObj == 0);
                } else {
                    print "Warning: can't handle varargs function '$name'\n";
                }
                next;
            }

	    # newXS special case
	    if ($name eq "newXS") {
		next;
	    }
            
            print OUTFILE "\n#ifdef $name" . "defined" unless ($separateObj == 0);

	    # handle specical case for save_destructor
	    if ($name eq "save_destructor") {
		next;
	    }
	    # handle specical case for sighandler
	    if ($name eq "sighandler") {
		next;
	    }
	    # handle special case for sv_grow
	    if ($name eq "sv_grow" and $args eq "SV* sv, unsigned long newlen") {
		next;
	    }
	    # handle special case for newSV
	    if ($name eq "newSV" and $args eq "I32 x, STRLEN len") {
		next;
	    }
	    # handle special case for perl_parse
	    if ($name eq "perl_parse") {
		print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $name ($args)
{
    return pPerl->perl_parse(xsinit, argc, argv, env);
}
ENDCODE
                print OUTFILE "#endif\n" unless ($separateObj == 0);
		next;
	    }
	    # handle special case for perl_atexit
	    if ($name eq "perl_atexit") {
		print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $name ($args)
{
    pPerl->perl_atexit(fn, ptr);
}
ENDCODE
                print OUTFILE "#endif\n" unless ($separateObj == 0);
		next;
	    }


	    if($name eq "byterun" and $args eq "struct bytestream bs") {
		next;
	    }

            # foo(void);
            if ($args eq "void") {
                print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $funcName ()
{
$return pPerl->$funcName();
}

ENDCODE
                print OUTFILE "#endif\n" unless ($separateObj == 0);
                next;
            }

            # foo(char *s, const int bar);
            print OUTFILE <<ENDCODE;

#undef $name
EXTERN_C $type $funcName ($args)
{
ENDCODE
	    print OUTFILE "$return pPerl->$funcName";
            $doneone = 0;
            foreach $arg (@args) {
                if ($arg =~ /(\w+)\W*$/) {
                    if ($doneone) {
                        print OUTFILE ", $1";
                    }
                    else {
                        print OUTFILE "($1";
                        $doneone++;
                    }
                }
            }
            print OUTFILE ");\n}\n";
            print OUTFILE "#endif\n" unless ($separateObj == 0);
        }
        else {
            print "failed to match $_";
        }
    }
}

close INFILE;

%skip_list = ();

skip_these [qw(
strchop
filemode
lastfd
oldname
curinterp
Argv
Cmd
sortcop
sortstash
firstgv
secondgv
sortstack
signalstack
mystrk
dumplvl
oldlastpm
gensym
preambled
preambleav
Ilaststatval
Ilaststype
mess_sv
ors
opsave
eval_mutex
strtab_mutex
orslen
ofmt
modcount
generation
DBcv
archpat_auto
sortcxix
lastgotoprobe
regdummy
regcomp_parse
regxend
regcode
regnaughty
regsawback
regprecomp
regnpar
regsize
regflags
regseen
seen_zerolen
regcomp_rx
extralen
colorset
colors
reginput
regbol
regeol
regstartp
regendp
reglastparen
regtill
regprev
reg_start_tmp
reg_start_tmpl
regdata
bostr
reg_flags
reg_eval_set
regnarrate
regprogram
regindent
regcc
in_clean_objs
in_clean_all
linestart
pending_ident
statusvalue_vms
sublex_info
thrsv
threadnum
PL_piMem
PL_piENV
PL_piStdIO
PL_piLIO
PL_piDir
PL_piSock
PL_piProc
cshname
threadsv_names
thread
nthreads
thr_key
threads_mutex
malloc_mutex
svref_mutex
sv_mutex
cred_mutex
nthreads_cond
eval_cond
cryptseen
cshlen
)];

sub readvars(\%$$) {
    my ($syms, $file, $pre) = @_;
    %$syms = ();
    local (*FILE, $_);
    open(FILE, "< $file")
	or die "$0: Can't open $file: $!\n";
    while (<FILE>) {
	s/[ \t]*#.*//;		# Delete comments.
	if (/PERLVARI?C?\($pre(\w+),\s*([^,)]+)/) {
	    $$syms{$1} = $2;
	}
    }
    close(FILE);
}

my %intrp;
my %thread;
my %globvar;

readvars %intrp,  '..\intrpvar.h','I';
readvars %thread, '..\thrdvar.h','T';
readvars %globvar, '..\perlvars.h','G';

open(HDRFILE, ">$hdrfile") or die "$0: Can't open $hdrfile: $!\n";
print HDRFILE <<ENDCODE;
EXTERN_C void SetCPerlObj(void* pP);
EXTERN_C CV* Perl_newXS(char* name, void (*subaddr)(CV* cv), char* filename);

ENDCODE

sub DoVariable($$) {
    my $name = shift;
    my $type = shift;

    return if (defined $skip_list{$name});
    return if ($type eq 'struct perl_thread *');

    print OUTFILE "\n#ifdef $name" . "_defined" unless ($separateObj == 0);
    print OUTFILE <<ENDCODE;
#undef PL_$name
EXTERN_C $type * _PL_$name ()
{
    return (($type *)&pPerl->PL_$name);
}

ENDCODE

    print OUTFILE "#endif\n" unless ($separateObj == 0);

    print HDRFILE <<ENDCODE;

#undef PL_$name
EXTERN_C $type * _PL_$name ();
#define PL_$name (*_PL_$name())

ENDCODE

}

foreach $key (keys %intrp) {
    DoVariable ($key, $intrp{$key});
}

foreach $key (keys %thread) {
    DoVariable ($key, $thread{$key});
}

foreach $key (keys %globvar) {
    DoVariable ($key, $globvar{$key});
}

print OUTFILE <<EOCODE;

START_EXTERN_C

char **	_Perl_op_desc(void)
{
    return pPerl->Perl_get_op_descs();
}

char **	_Perl_op_name(void)
{
    return pPerl->Perl_get_op_names();
}

char *	_Perl_no_modify(void)
{
    return pPerl->Perl_get_no_modify();
}

U32 *	_Perl_opargs(void)
{
    return pPerl->Perl_get_opargs();
}

void xs_handler(CV* cv, CPerlObj* p)
{
#ifndef NO_XSLOCKS
    XSLock localLock(p);
#endif
    void(*func)(CV*);
    SV* sv;
    MAGIC* m = pPerl->Perl_mg_find((SV*)cv, '~');
    if(m != NULL)
    {
	sv = m->mg_obj;
	if(SvIOK(sv))
	{
	    func = (void(*)(CV*))SvIVX(sv);
	}
	else
	{
	    func = (void(*)(CV*))pPerl->Perl_sv_2iv(sv);
	}
	func(cv);
    }
}

EXTERN_C CV* Perl_newXS(char* name, void (*subaddr)(CV* cv), char* filename)
{
    CV* cv = pPerl->Perl_newXS(name, xs_handler, filename);
    pPerl->Perl_sv_magic((SV*)cv, pPerl->Perl_sv_2mortal(pPerl->Perl_newSViv((IV)subaddr)), '~', "CAPI", 4);
    return cv;
}


void Perl_deb(const char pat, ...)
{
}

#undef PL_piMem
#undef PL_piENV
#undef PL_piStdIO
#undef PL_piLIO
#undef PL_piDir
#undef PL_piSock
#undef PL_piProc

int *        _win32_errno(void)
{
    return &pPerl->ErrorNo();
}

FILE*        _win32_stdin(void)
{
    return (FILE*)pPerl->PL_piStdIO->Stdin();
}

FILE*        _win32_stdout(void)
{
    return (FILE*)pPerl->PL_piStdIO->Stdout();
}

FILE*        _win32_stderr(void)
{
    return (FILE*)pPerl->PL_piStdIO->Stderr();
}

int          _win32_ferror(FILE *fp)
{
    return pPerl->PL_piStdIO->Error((PerlIO*)fp, ErrorNo());
}

int          _win32_feof(FILE *fp)
{
    return pPerl->PL_piStdIO->Eof((PerlIO*)fp, ErrorNo());
}

char*	     _win32_strerror(int e)
{
    return strerror(e);
}

void	     _win32_perror(const char *str)
{
    perror(str);
}

int          _win32_vfprintf(FILE *pf, const char *format, va_list arg)
{
    return pPerl->PL_piStdIO->Vprintf((PerlIO*)pf, ErrorNo(), format, arg);
}

int          _win32_vprintf(const char *format, va_list arg)
{
    return pPerl->PL_piStdIO->Vprintf(pPerl->PL_piStdIO->Stdout(), ErrorNo(), format, arg);
}

int          _win32_fprintf(FILE *pf, const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = _win32_vfprintf(pf, format, args);
    va_end(args);
    return ret;
}

int          _win32_printf(const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = _win32_vprintf(format, args);
    va_end(args);
    return ret;
}

size_t       _win32_fread(void *buf, size_t size, size_t count, FILE *pf)
{
    return pPerl->PL_piStdIO->Read((PerlIO*)pf, buf, (size*count), ErrorNo());
}

size_t       _win32_fwrite(const void *buf, size_t size, size_t count, FILE *pf)
{
    return pPerl->PL_piStdIO->Write((PerlIO*)pf, buf, (size*count), ErrorNo());
}

FILE*        _win32_fopen(const char *path, const char *mode)
{
    return (FILE*)pPerl->PL_piStdIO->Open(path, mode, ErrorNo());
}

FILE*        _win32_fdopen(int fh, const char *mode)
{
    return (FILE*)pPerl->PL_piStdIO->Fdopen(fh, mode, ErrorNo());
}

FILE*        _win32_freopen(const char *path, const char *mode, FILE *pf)
{
    return (FILE*)pPerl->PL_piStdIO->Reopen(path, mode, (PerlIO*)pf, ErrorNo());
}

int          _win32_fclose(FILE *pf)
{
    return pPerl->PL_piStdIO->Close((PerlIO*)pf, ErrorNo());
}

int          _win32_fputs(const char *s,FILE *pf)
{
    return pPerl->PL_piStdIO->Puts((PerlIO*)pf, s, ErrorNo());
}

int          _win32_fputc(int c,FILE *pf)
{
    return pPerl->PL_piStdIO->Putc((PerlIO*)pf, c, ErrorNo());
}

int          _win32_ungetc(int c,FILE *pf)
{
    return pPerl->PL_piStdIO->Ungetc((PerlIO*)pf, c, ErrorNo());
}

int          _win32_getc(FILE *pf)
{
    return pPerl->PL_piStdIO->Getc((PerlIO*)pf, ErrorNo());
}

int          _win32_fileno(FILE *pf)
{
    return pPerl->PL_piStdIO->Fileno((PerlIO*)pf, ErrorNo());
}

void         _win32_clearerr(FILE *pf)
{
    pPerl->PL_piStdIO->Clearerr((PerlIO*)pf, ErrorNo());
}

int          _win32_fflush(FILE *pf)
{
    return pPerl->PL_piStdIO->Flush((PerlIO*)pf, ErrorNo());
}

long         _win32_ftell(FILE *pf)
{
    return pPerl->PL_piStdIO->Tell((PerlIO*)pf, ErrorNo());
}

int          _win32_fseek(FILE *pf,long offset,int origin)
{
    return pPerl->PL_piStdIO->Seek((PerlIO*)pf, offset, origin, ErrorNo());
}

int          _win32_fgetpos(FILE *pf,fpos_t *p)
{
    return pPerl->PL_piStdIO->Getpos((PerlIO*)pf, p, ErrorNo());
}

int          _win32_fsetpos(FILE *pf,const fpos_t *p)
{
    return pPerl->PL_piStdIO->Setpos((PerlIO*)pf, p, ErrorNo());
}

void         _win32_rewind(FILE *pf)
{
    pPerl->PL_piStdIO->Rewind((PerlIO*)pf, ErrorNo());
}

FILE*        _win32_tmpfile(void)
{
    return (FILE*)pPerl->PL_piStdIO->Tmpfile(ErrorNo());
}

void         _win32_setbuf(FILE *pf, char *buf)
{
    pPerl->PL_piStdIO->SetBuf((PerlIO*)pf, buf, ErrorNo());
}

int          _win32_setvbuf(FILE *pf, char *buf, int type, size_t size)
{
    return pPerl->PL_piStdIO->SetVBuf((PerlIO*)pf, buf, type, size, ErrorNo());
}

char*		_win32_fgets(char *s, int n, FILE *pf)
{
    return pPerl->PL_piStdIO->Gets((PerlIO*)pf, s, n, ErrorNo());
}

char*		_win32_gets(char *s)
{
    return _win32_fgets(s, 80, (FILE*)pPerl->PL_piStdIO->Stdin());
}

int          _win32_fgetc(FILE *pf)
{
    return pPerl->PL_piStdIO->Getc((PerlIO*)pf, ErrorNo());
}

int          _win32_putc(int c, FILE *pf)
{
    return pPerl->PL_piStdIO->Putc((PerlIO*)pf, c, ErrorNo());
}

int          _win32_puts(const char *s)
{
    return pPerl->PL_piStdIO->Puts(pPerl->PL_piStdIO->Stdout(), s, ErrorNo());
}

int          _win32_getchar(void)
{
    return pPerl->PL_piStdIO->Getc(pPerl->PL_piStdIO->Stdin(), ErrorNo());
}

int          _win32_putchar(int c)
{
    return pPerl->PL_piStdIO->Putc(pPerl->PL_piStdIO->Stdout(), c, ErrorNo());
}

void*        _win32_malloc(size_t size)
{
    return pPerl->PL_piMem->Malloc(size);
}

void*        _win32_calloc(size_t numitems, size_t size)
{
    return pPerl->PL_piMem->Malloc(numitems*size);
}

void*        _win32_realloc(void *block, size_t size)
{
    return pPerl->PL_piMem->Realloc(block, size);
}

void         _win32_free(void *block)
{
    pPerl->PL_piMem->Free(block);
}

void         _win32_abort(void)
{
    pPerl->PL_piProc->Abort();
}

int          _win32_pipe(int *phandles, unsigned int psize, int textmode)
{
    return pPerl->PL_piProc->Pipe(phandles);
}

FILE*        _win32_popen(const char *command, const char *mode)
{
    return (FILE*)pPerl->PL_piProc->Popen(command, mode);
}

int          _win32_pclose(FILE *pf)
{
    return pPerl->PL_piProc->Pclose((PerlIO*)pf);
}

unsigned     _win32_sleep(unsigned int t)
{
    return pPerl->PL_piProc->Sleep(t);
}

int	_win32_spawnvp(int mode, const char *cmdname, const char *const *argv)
{
    return pPerl->PL_piProc->Spawnvp(mode, cmdname, argv);
}

int          _win32_mkdir(const char *dir, int mode)
{
    return pPerl->PL_piDir->Makedir(dir, mode, ErrorNo());
}

int          _win32_rmdir(const char *dir)
{
    return pPerl->PL_piDir->Rmdir(dir, ErrorNo());
}

int          _win32_chdir(const char *dir)
{
    return pPerl->PL_piDir->Chdir(dir, ErrorNo());
}

#undef stat
int          _win32_fstat(int fd,struct stat *sbufptr)
{
    return pPerl->PL_piLIO->FileStat(fd, sbufptr, ErrorNo());
}

int          _win32_stat(const char *name,struct stat *sbufptr)
{
    return pPerl->PL_piLIO->NameStat(name, sbufptr, ErrorNo());
}

int          _win32_rename(const char *oname, const char *newname)
{
    return pPerl->PL_piLIO->Rename(oname, newname, ErrorNo());
}

int          _win32_setmode(int fd, int mode)
{
    return pPerl->PL_piLIO->Setmode(fd, mode, ErrorNo());
}

long         _win32_lseek(int fd, long offset, int origin)
{
    return pPerl->PL_piLIO->Lseek(fd, offset, origin, ErrorNo());
}

long         _win32_tell(int fd)
{
    return pPerl->PL_piStdIO->Tell((PerlIO*)fd, ErrorNo());
}

int          _win32_dup(int fd)
{
    return pPerl->PL_piLIO->Dup(fd, ErrorNo());
}

int          _win32_dup2(int h1, int h2)
{
    return pPerl->PL_piLIO->Dup2(h1, h2, ErrorNo());
}

int          _win32_open(const char *path, int oflag,...)
{
    return pPerl->PL_piLIO->Open(path, oflag, ErrorNo());
}

int          _win32_close(int fd)
{
    return pPerl->PL_piLIO->Close(fd, ErrorNo());
}

int          _win32_read(int fd, void *buf, unsigned int cnt)
{
    return pPerl->PL_piLIO->Read(fd, buf, cnt, ErrorNo());
}

int          _win32_write(int fd, const void *buf, unsigned int cnt)
{
    return pPerl->PL_piLIO->Write(fd, buf, cnt, ErrorNo());
}

int          _win32_times(struct tms *timebuf)
{
    return pPerl->PL_piProc->Times(timebuf);
}

int          _win32_ioctl(int i, unsigned int u, char *data)
{
    return pPerl->PL_piLIO->IOCtl(i, u, data, ErrorNo());
}

int          _win32_utime(const char *f, struct utimbuf *t)
{
    return pPerl->PL_piLIO->Utime((char*)f, t, ErrorNo());
}

char*   _win32_getenv(const char *name)
{
    return pPerl->PL_piENV->Getenv(name, ErrorNo());
}

int   _win32_putenv(const char *name)
{
    return pPerl->PL_piENV->Putenv(name, ErrorNo());
}

int          _win32_open_osfhandle(long handle, int flags)
{
    return pPerl->PL_piStdIO->OpenOSfhandle(handle, flags);
}

long         _win32_get_osfhandle(int fd)
{
    return pPerl->PL_piStdIO->GetOSfhandle(fd);
}

u_long _win32_htonl (u_long hostlong)
{
    return pPerl->PL_piSock->Htonl(hostlong);
}

u_short _win32_htons (u_short hostshort)
{
    return pPerl->PL_piSock->Htons(hostshort);
}

u_long _win32_ntohl (u_long netlong)
{
    return pPerl->PL_piSock->Ntohl(netlong);
}

u_short _win32_ntohs (u_short netshort)
{
    return pPerl->PL_piSock->Ntohs(netshort);
}

unsigned long _win32_inet_addr (const char * cp)
{
    return pPerl->PL_piSock->InetAddr(cp, ErrorNo());
}

char * _win32_inet_ntoa (struct in_addr in)
{
    return pPerl->PL_piSock->InetNtoa(in, ErrorNo());
}

SOCKET _win32_socket (int af, int type, int protocol)
{
    return pPerl->PL_piSock->Socket(af, type, protocol, ErrorNo());
}

int _win32_bind (SOCKET s, const struct sockaddr *addr, int namelen)
{
    return pPerl->PL_piSock->Bind(s, addr, namelen, ErrorNo());
}

int _win32_listen (SOCKET s, int backlog)
{
    return pPerl->PL_piSock->Listen(s, backlog, ErrorNo());
}

SOCKET _win32_accept (SOCKET s, struct sockaddr *addr, int *addrlen)
{
    return pPerl->PL_piSock->Accept(s, addr, addrlen, ErrorNo());
}

int _win32_connect (SOCKET s, const struct sockaddr *name, int namelen)
{
    return pPerl->PL_piSock->Connect(s, name, namelen, ErrorNo());
}

int _win32_send (SOCKET s, const char * buf, int len, int flags)
{
    return pPerl->PL_piSock->Send(s, buf, len, flags, ErrorNo());
}

int _win32_sendto (SOCKET s, const char * buf, int len, int flags,
                       const struct sockaddr *to, int tolen)
{
    return pPerl->PL_piSock->Sendto(s, buf, len, flags, to, tolen, ErrorNo());
}

int _win32_recv (SOCKET s, char * buf, int len, int flags)
{
    return pPerl->PL_piSock->Recv(s, buf, len, flags, ErrorNo());
}

int _win32_recvfrom (SOCKET s, char * buf, int len, int flags,
                         struct sockaddr *from, int * fromlen)
{
    return pPerl->PL_piSock->Recvfrom(s, buf, len, flags, from, fromlen, ErrorNo());
}

int _win32_shutdown (SOCKET s, int how)
{
    return pPerl->PL_piSock->Shutdown(s, how, ErrorNo());
}

int _win32_closesocket (SOCKET s)
{
    return pPerl->PL_piSock->Closesocket(s, ErrorNo());
}

int _win32_ioctlsocket (SOCKET s, long cmd, u_long *argp)
{
    return pPerl->PL_piSock->Ioctlsocket(s, cmd, argp, ErrorNo());
}

int _win32_setsockopt (SOCKET s, int level, int optname,
                           const char * optval, int optlen)
{
    return pPerl->PL_piSock->Setsockopt(s, level, optname, optval, optlen, ErrorNo());
}

int _win32_getsockopt (SOCKET s, int level, int optname, char * optval, int *optlen)
{
    return pPerl->PL_piSock->Getsockopt(s, level, optname, optval, optlen, ErrorNo());
}

int _win32_getpeername (SOCKET s, struct sockaddr *name, int * namelen)
{
    return pPerl->PL_piSock->Getpeername(s, name, namelen, ErrorNo());
}

int _win32_getsockname (SOCKET s, struct sockaddr *name, int * namelen)
{
    return pPerl->PL_piSock->Getsockname(s, name, namelen, ErrorNo());
}

int _win32_gethostname (char * name, int namelen)
{
    return pPerl->PL_piSock->Gethostname(name, namelen, ErrorNo());
}

struct hostent * _win32_gethostbyname(const char * name)
{
    return pPerl->PL_piSock->Gethostbyname(name, ErrorNo());
}

struct hostent * _win32_gethostbyaddr(const char * addr, int len, int type)
{
    return pPerl->PL_piSock->Gethostbyaddr(addr, len, type, ErrorNo());
}

struct protoent * _win32_getprotobyname(const char * name)
{
    return pPerl->PL_piSock->Getprotobyname(name, ErrorNo());
}

struct protoent * _win32_getprotobynumber(int proto)
{
    return pPerl->PL_piSock->Getprotobynumber(proto, ErrorNo());
}

struct servent * _win32_getservbyname(const char * name, const char * proto)
{
    return pPerl->PL_piSock->Getservbyname(name, proto, ErrorNo());
}

struct servent * _win32_getservbyport(int port, const char * proto)
{
    return pPerl->PL_piSock->Getservbyport(port, proto, ErrorNo());
}

int _win32_select (int nfds, Perl_fd_set *rfds, Perl_fd_set *wfds, Perl_fd_set *xfds,
		  const struct timeval *timeout)
{
    return pPerl->PL_piSock->Select(nfds, (char*)rfds, (char*)wfds, (char*)xfds, timeout, ErrorNo());
}

void _win32_endnetent(void)
{
    pPerl->PL_piSock->Endnetent(ErrorNo());
}

void _win32_endhostent(void)
{
    pPerl->PL_piSock->Endhostent(ErrorNo());
}

void _win32_endprotoent(void)
{
    pPerl->PL_piSock->Endprotoent(ErrorNo());
}

void _win32_endservent(void)
{
    pPerl->PL_piSock->Endservent(ErrorNo());
}

struct netent * _win32_getnetent(void)
{
    return pPerl->PL_piSock->Getnetent(ErrorNo());
}

struct netent * _win32_getnetbyname(char *name)
{
    return pPerl->PL_piSock->Getnetbyname(name, ErrorNo());
}

struct netent * _win32_getnetbyaddr(long net, int type)
{
    return pPerl->PL_piSock->Getnetbyaddr(net, type, ErrorNo());
}

struct protoent *_win32_getprotoent(void)
{
    return pPerl->PL_piSock->Getprotoent(ErrorNo());
}

struct servent *_win32_getservent(void)
{
    return pPerl->PL_piSock->Getservent(ErrorNo());
}

void _win32_sethostent(int stayopen)
{
    pPerl->PL_piSock->Sethostent(stayopen, ErrorNo());
}

void _win32_setnetent(int stayopen)
{
    pPerl->PL_piSock->Setnetent(stayopen, ErrorNo());
}

void _win32_setprotoent(int stayopen)
{
    pPerl->PL_piSock->Setprotoent(stayopen, ErrorNo());
}

void _win32_setservent(int stayopen)
{
    pPerl->PL_piSock->Setservent(stayopen, ErrorNo());
}
END_EXTERN_C
EOCODE


print HDRFILE <<EOCODE;

START_EXTERN_C

#undef Perl_op_desc
char ** _Perl_op_desc ();
#define Perl_op_desc (_Perl_op_desc())

#undef Perl_op_name
char ** _Perl_op_name ();
#define Perl_op_name (_Perl_op_name())

#undef Perl_no_modify
char * _Perl_no_modify ();
#define Perl_no_modify (_Perl_no_modify())

#undef Perl_opargs
U32 * _Perl_opargs ();
#define Perl_opargs (_Perl_opargs())


#undef win32_errno
#undef win32_stdin
#undef win32_stdout
#undef win32_stderr
#undef win32_ferror
#undef win32_feof
#undef win32_fprintf
#undef win32_printf
#undef win32_vfprintf
#undef win32_vprintf
#undef win32_fread
#undef win32_fwrite
#undef win32_fopen
#undef win32_fdopen
#undef win32_freopen
#undef win32_fclose
#undef win32_fputs
#undef win32_fputc
#undef win32_ungetc
#undef win32_getc
#undef win32_fileno
#undef win32_clearerr
#undef win32_fflush
#undef win32_ftell
#undef win32_fseek
#undef win32_fgetpos
#undef win32_fsetpos
#undef win32_rewind
#undef win32_tmpfile
#undef win32_abort
#undef win32_fstat
#undef win32_stat
#undef win32_pipe
#undef win32_popen
#undef win32_pclose
#undef win32_rename
#undef win32_setmode
#undef win32_lseek
#undef win32_tell
#undef win32_dup
#undef win32_dup2
#undef win32_open
#undef win32_close
#undef win32_eof
#undef win32_read
#undef win32_write
#undef win32_mkdir
#undef win32_rmdir
#undef win32_chdir
#undef win32_setbuf
#undef win32_setvbuf
#undef win32_fgetc
#undef win32_fgets
#undef win32_gets
#undef win32_putc
#undef win32_puts
#undef win32_getchar
#undef win32_putchar
#undef win32_malloc
#undef win32_calloc
#undef win32_realloc
#undef win32_free
#undef win32_sleep
#undef win32_times
#undef win32_stat
#undef win32_ioctl
#undef win32_utime
#undef win32_getenv

#undef win32_htonl
#undef win32_htons
#undef win32_ntohl
#undef win32_ntohs
#undef win32_inet_addr
#undef win32_inet_ntoa

#undef win32_socket
#undef win32_bind
#undef win32_listen
#undef win32_accept
#undef win32_connect
#undef win32_send
#undef win32_sendto
#undef win32_recv
#undef win32_recvfrom
#undef win32_shutdown
#undef win32_closesocket
#undef win32_ioctlsocket
#undef win32_setsockopt
#undef win32_getsockopt
#undef win32_getpeername
#undef win32_getsockname
#undef win32_gethostname
#undef win32_gethostbyname
#undef win32_gethostbyaddr
#undef win32_getprotobyname
#undef win32_getprotobynumber
#undef win32_getservbyname
#undef win32_getservbyport
#undef win32_select
#undef win32_endhostent
#undef win32_endnetent
#undef win32_endprotoent
#undef win32_endservent
#undef win32_getnetent
#undef win32_getnetbyname
#undef win32_getnetbyaddr
#undef win32_getprotoent
#undef win32_getservent
#undef win32_sethostent
#undef win32_setnetent
#undef win32_setprotoent
#undef win32_setservent

#define win32_errno    _win32_errno
#define win32_stdin    _win32_stdin
#define win32_stdout   _win32_stdout
#define win32_stderr   _win32_stderr
#define win32_ferror   _win32_ferror
#define win32_feof     _win32_feof
#define win32_strerror _win32_strerror
#define win32_perror   _win32_perror
#define win32_fprintf  _win32_fprintf
#define win32_printf   _win32_printf
#define win32_vfprintf _win32_vfprintf
#define win32_vprintf  _win32_vprintf
#define win32_fread    _win32_fread
#define win32_fwrite   _win32_fwrite
#define win32_fopen    _win32_fopen
#define win32_fdopen   _win32_fdopen
#define win32_freopen  _win32_freopen
#define win32_fclose   _win32_fclose
#define win32_fputs    _win32_fputs
#define win32_fputc    _win32_fputc
#define win32_ungetc   _win32_ungetc
#define win32_getc     _win32_getc
#define win32_fileno   _win32_fileno
#define win32_clearerr _win32_clearerr
#define win32_fflush   _win32_fflush
#define win32_ftell    _win32_ftell
#define win32_fseek    _win32_fseek
#define win32_fgetpos  _win32_fgetpos
#define win32_fsetpos  _win32_fsetpos
#define win32_rewind   _win32_rewind
#define win32_tmpfile  _win32_tmpfile
#define win32_abort    _win32_abort
#define win32_fstat    _win32_fstat
#define win32_stat     _win32_stat
#define win32_pipe     _win32_pipe
#define win32_popen    _win32_popen
#define win32_pclose   _win32_pclose
#define win32_rename   _win32_rename
#define win32_setmode  _win32_setmode
#define win32_lseek    _win32_lseek
#define win32_tell     _win32_tell
#define win32_dup      _win32_dup
#define win32_dup2     _win32_dup2
#define win32_open     _win32_open
#define win32_close    _win32_close
#define win32_eof      _win32_eof
#define win32_read     _win32_read
#define win32_write    _win32_write
#define win32_mkdir    _win32_mkdir
#define win32_rmdir    _win32_rmdir
#define win32_chdir    _win32_chdir
#define win32_setbuf   _win32_setbuf
#define win32_setvbuf  _win32_setvbuf
#define win32_fgetc    _win32_fgetc
#define win32_fgets    _win32_fgets
#define win32_gets     _win32_gets
#define win32_putc     _win32_putc
#define win32_puts     _win32_puts
#define win32_getchar  _win32_getchar
#define win32_putchar  _win32_putchar
#define win32_malloc   _win32_malloc
#define win32_calloc   _win32_calloc
#define win32_realloc  _win32_realloc
#define win32_free     _win32_free
#define win32_sleep    _win32_sleep
#define win32_spawnvp  _win32_spawnvp
#define win32_times    _win32_times
#define win32_stat     _win32_stat
#define win32_ioctl    _win32_ioctl
#define win32_utime    _win32_utime
#define win32_getenv   _win32_getenv
#define win32_open_osfhandle _win32_open_osfhandle
#define win32_get_osfhandle  _win32_get_osfhandle

#define win32_htonl              _win32_htonl
#define win32_htons              _win32_htons
#define win32_ntohl              _win32_ntohl
#define win32_ntohs              _win32_ntohs
#define win32_inet_addr          _win32_inet_addr
#define win32_inet_ntoa          _win32_inet_ntoa

#define win32_socket             _win32_socket
#define win32_bind               _win32_bind
#define win32_listen             _win32_listen
#define win32_accept             _win32_accept
#define win32_connect            _win32_connect
#define win32_send               _win32_send
#define win32_sendto             _win32_sendto
#define win32_recv               _win32_recv
#define win32_recvfrom           _win32_recvfrom
#define win32_shutdown           _win32_shutdown
#define win32_closesocket        _win32_closesocket
#define win32_ioctlsocket        _win32_ioctlsocket
#define win32_setsockopt         _win32_setsockopt
#define win32_getsockopt         _win32_getsockopt
#define win32_getpeername        _win32_getpeername
#define win32_getsockname        _win32_getsockname
#define win32_gethostname        _win32_gethostname
#define win32_gethostbyname      _win32_gethostbyname
#define win32_gethostbyaddr      _win32_gethostbyaddr
#define win32_getprotobyname     _win32_getprotobyname
#define win32_getprotobynumber   _win32_getprotobynumber
#define win32_getservbyname      _win32_getservbyname
#define win32_getservbyport      _win32_getservbyport
#define win32_select             _win32_select
#define win32_endhostent         _win32_endhostent
#define win32_endnetent          _win32_endnetent
#define win32_endprotoent        _win32_endprotoent
#define win32_endservent         _win32_endservent
#define win32_getnetent          _win32_getnetent
#define win32_getnetbyname       _win32_getnetbyname
#define win32_getnetbyaddr       _win32_getnetbyaddr
#define win32_getprotoent        _win32_getprotoent
#define win32_getservent         _win32_getservent
#define win32_sethostent         _win32_sethostent
#define win32_setnetent          _win32_setnetent
#define win32_setprotoent        _win32_setprotoent
#define win32_setservent         _win32_setservent

int * 	_win32_errno(void);
FILE*	_win32_stdin(void);
FILE*	_win32_stdout(void);
FILE*	_win32_stderr(void);
int	_win32_ferror(FILE *fp);
int	_win32_feof(FILE *fp);
char*	_win32_strerror(int e);
void    _win32_perror(const char *str);
int	_win32_fprintf(FILE *pf, const char *format, ...);
int	_win32_printf(const char *format, ...);
int	_win32_vfprintf(FILE *pf, const char *format, va_list arg);
int	_win32_vprintf(const char *format, va_list arg);
size_t	_win32_fread(void *buf, size_t size, size_t count, FILE *pf);
size_t	_win32_fwrite(const void *buf, size_t size, size_t count, FILE *pf);
FILE*	_win32_fopen(const char *path, const char *mode);
FILE*	_win32_fdopen(int fh, const char *mode);
FILE*	_win32_freopen(const char *path, const char *mode, FILE *pf);
int	_win32_fclose(FILE *pf);
int	_win32_fputs(const char *s,FILE *pf);
int	_win32_fputc(int c,FILE *pf);
int	_win32_ungetc(int c,FILE *pf);
int	_win32_getc(FILE *pf);
int	_win32_fileno(FILE *pf);
void	_win32_clearerr(FILE *pf);
int	_win32_fflush(FILE *pf);
long	_win32_ftell(FILE *pf);
int	_win32_fseek(FILE *pf,long offset,int origin);
int	_win32_fgetpos(FILE *pf,fpos_t *p);
int	_win32_fsetpos(FILE *pf,const fpos_t *p);
void	_win32_rewind(FILE *pf);
FILE*	_win32_tmpfile(void);
void	_win32_abort(void);
int  	_win32_fstat(int fd,struct stat *sbufptr);
int  	_win32_stat(const char *name,struct stat *sbufptr);
int	_win32_pipe( int *phandles, unsigned int psize, int textmode );
FILE*	_win32_popen( const char *command, const char *mode );
int	_win32_pclose( FILE *pf);
int	_win32_rename( const char *oldname, const char *newname);
int	_win32_setmode( int fd, int mode);
long	_win32_lseek( int fd, long offset, int origin);
long	_win32_tell( int fd);
int	_win32_dup( int fd);
int	_win32_dup2(int h1, int h2);
int	_win32_open(const char *path, int oflag,...);
int	_win32_close(int fd);
int	_win32_eof(int fd);
int	_win32_read(int fd, void *buf, unsigned int cnt);
int	_win32_write(int fd, const void *buf, unsigned int cnt);
int	_win32_mkdir(const char *dir, int mode);
int	_win32_rmdir(const char *dir);
int	_win32_chdir(const char *dir);
void	_win32_setbuf(FILE *pf, char *buf);
int	_win32_setvbuf(FILE *pf, char *buf, int type, size_t size);
char*	_win32_fgets(char *s, int n, FILE *pf);
char*	_win32_gets(char *s);
int	_win32_fgetc(FILE *pf);
int	_win32_putc(int c, FILE *pf);
int	_win32_puts(const char *s);
int	_win32_getchar(void);
int	_win32_putchar(int c);
void*	_win32_malloc(size_t size);
void*	_win32_calloc(size_t numitems, size_t size);
void*	_win32_realloc(void *block, size_t size);
void	_win32_free(void *block);
unsigned _win32_sleep(unsigned int);
int	_win32_spawnvp(int mode, const char *cmdname, const char *const *argv);
int	_win32_times(struct tms *timebuf);
int	_win32_stat(const char *path, struct stat *buf);
int	_win32_ioctl(int i, unsigned int u, char *data);
int	_win32_utime(const char *f, struct utimbuf *t);
char*   _win32_getenv(const char *name);
int     _win32_open_osfhandle(long handle, int flags);
long    _win32_get_osfhandle(int fd);

u_long _win32_htonl (u_long hostlong);
u_short _win32_htons (u_short hostshort);
u_long _win32_ntohl (u_long netlong);
u_short _win32_ntohs (u_short netshort);
unsigned long _win32_inet_addr (const char * cp);
char * _win32_inet_ntoa (struct in_addr in);

SOCKET _win32_socket (int af, int type, int protocol);
int _win32_bind (SOCKET s, const struct sockaddr *addr, int namelen);
int _win32_listen (SOCKET s, int backlog);
SOCKET _win32_accept (SOCKET s, struct sockaddr *addr, int *addrlen);
int _win32_connect (SOCKET s, const struct sockaddr *name, int namelen);
int _win32_send (SOCKET s, const char * buf, int len, int flags);
int _win32_sendto (SOCKET s, const char * buf, int len, int flags,
                       const struct sockaddr *to, int tolen);
int _win32_recv (SOCKET s, char * buf, int len, int flags);
int _win32_recvfrom (SOCKET s, char * buf, int len, int flags,
                         struct sockaddr *from, int * fromlen);
int _win32_shutdown (SOCKET s, int how);
int _win32_closesocket (SOCKET s);
int _win32_ioctlsocket (SOCKET s, long cmd, u_long *argp);
int _win32_setsockopt (SOCKET s, int level, int optname,
                           const char * optval, int optlen);
int _win32_getsockopt (SOCKET s, int level, int optname, char * optval, int *optlen);
int _win32_getpeername (SOCKET s, struct sockaddr *name, int * namelen);
int _win32_getsockname (SOCKET s, struct sockaddr *name, int * namelen);
int _win32_gethostname (char * name, int namelen);
struct hostent * _win32_gethostbyname(const char * name);
struct hostent * _win32_gethostbyaddr(const char * addr, int len, int type);
struct protoent * _win32_getprotobyname(const char * name);
struct protoent * _win32_getprotobynumber(int proto);
struct servent * _win32_getservbyname(const char * name, const char * proto);
struct servent * _win32_getservbyport(int port, const char * proto);
int _win32_select (int nfds, Perl_fd_set *rfds, Perl_fd_set *wfds, Perl_fd_set *xfds,
		  const struct timeval *timeout);
void _win32_endnetent(void);
void _win32_endhostent(void);
void _win32_endprotoent(void);
void _win32_endservent(void);
struct netent * _win32_getnetent(void);
struct netent * _win32_getnetbyname(char *name);
struct netent * _win32_getnetbyaddr(long net, int type);
struct protoent *_win32_getprotoent(void);
struct servent *_win32_getservent(void);
void _win32_sethostent(int stayopen);
void _win32_setnetent(int stayopen);
void _win32_setprotoent(int stayopen);
void _win32_setservent(int stayopen);

END_EXTERN_C

#pragma warning(once : 4113)
EOCODE


close HDRFILE;
close OUTFILE;
