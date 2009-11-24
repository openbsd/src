$set 1
$ EPERM
1 Handling ikke tilladt
$ ENOENT
2 Filen eller biblioteket findes ikke
$ ESRCH
3 Denna process findes ikke
$ EINTR
4 Systemkald afbrudt
$ EIO
5 I/O-fejl
$ ENXIO
6 Enheden er ikke konfigureret
$ E2BIG
7 Argumentlisten er for lang
$ ENOEXEC
8 Ikke-eksekverbar fil
$ EBADF
9 Defekt fildeskriptor
$ ECHILD
10 Ingen barneprocesser
$ EDEADLK
11 Undgik baglås
$ ENOMEM
12 Ingen hukommelse
$ EACCES
13 Adgang forbudt
$ EFAULT
14 Fejlagtig adresse
$ ENOTBLK
15 Blokenhed kræves
$ EBUSY
16 Enheden er optaget
$ EEXIST
17 Filen findes i forvejen
$ EXDEV
18 Linket krydser enheder
$ ENODEV
19 Enheden understøtter ikke operationen
$ ENOTDIR
20 Er ikke et bibliotek
$ EISDIR
21 Er et bibliotek
$ EINVAL
22 Ugyldigt argument
$ ENFILE
23 For mange åbne filer i systemet
$ EMFILE
24 For mange åbne filer
$ ENOTTY
25 Ikke-understøttet ioctl
$ ETXTBSY
26 Programfilen er optaget
$ EFBIG
27 Filen er for stor
$ ENOSPC
28 Ingen plads på enheden
$ ESPIPE
29 Ikke-tilladt søgning
$ EROFS
30 Skrivebeskyttet filsystem
$ EMLINK
31 For mange links
$ EPIPE
32 Afbrudt kommunikationskanal
$ EDOM
33 Numerisk argument udenfor definitionsmængden
$ ERANGE
34 Resultatet er for stort
$ EAGAIN, EWOULDBLOCK
35 Ressourcen er midlertidigt utilgængelig
$ EINPROGRESS
36 Operationen er i gang
$ EALREADY
37 Operationen ar allerede i gang
$ ENOTSOCK
38 Socketoperation på ikke-socket
$ EDESTADDRREQ
39 Destinationsadresse kræves
$ EMSGSIZE
40 For lang besked
$ EPROTOTYPE
41 Forkert protokoltype for socket
$ ENOPROTOOPT
42 Protokollen ikke tilgængelig
$ EPROTONOSUPPORT
43 Protokollen ikke understøttet
$ ESOCKTNOSUPPORT
44 Sockettypen ikke understøttet
$ EOPNOTSUPP
45 Operationen ikke understøttet
$ EPFNOSUPPORT
46 Protokolfamilien ikke understøttet
$ EAFNOSUPPORT
47 Adressefamilien ikke understøttet af protokolfamilien
$ EADDRINUSE
48 Adressen er optaget
$ EADDRNOTAVAIL
49 Kan ikke tildele den ønskede adresse
$ ENETDOWN
50 Netværket ude af drift
$ ENETUNREACH
51 Netværket er utilgængeligt
$ ENETRESET
52 Netværket mistede forbindelsen ved oprettelsen
$ ECONNABORTED
53 Programmel forårsagede forbindelsestab
$ ECONNRESET
54 Modparten afbrød forbindelsen
$ ENOBUFS
55 Ingen bufferplads tilgængelig
$ EISCONN
56 Socketen allerede forbundet
$ ENOTCONN
57 Socketen ikke forbundet
$ ESHUTDOWN
58 Kan ikke sende efter nedlukning af socket
$ ETOOMANYREFS
59 For mange referencer: kan ikke dele
$ ETIMEDOUT
60 Forbindelsen ikke forbundet i tide
$ ECONNREFUSED
61 Forbindelsen afvist
$ ELOOP
62 For mange niveauer af symbolske links
$ ENAMETOOLONG
63 Filnavn for langt
$ EHOSTDOWN
64 Værten er nede
$ EHOSTUNREACH
65 Værten kan ikke nås
$ ENOTEMPTY
66 Biblioteket ikke tomt
$ EPROCLIM
67 For mange processer
$ EUSERS
68 For mange brugere
$ EDQUOT
69 Diskkvote overskredet
$ ESTALE
70 Forældet NFS-filreference
$ EREMOTE
71 For mange fjernniveauer i stien
$ EBADRPC
72 Defekt RPC-struktur
$ ERPCMISMATCH
73 Defekt RPC-version
$ EPROGUNAVAIL
74 RPC-programmet utilgængeligt
$ EPROGMISMATCH
75 Defekt programversion
$ EPROCUNAVAIL
76 Fejlagtig procedure for programmet
$ ENOLCK
77 Ingen tilgængelige låse
$ ENOSYS
78 Funktionen ikke implementeret
$ EFTYPE
79 Upassende filtype eller format
$ EAUTH
80 Autentifikationsfejl
$ ENEEDAUTH
81 Behøver autentifikator
$ EIPSEC
82 IPsec-processeringsfejl
$ NOATTR
83 Attribute ikke fundet
$ EILSEQ
84 Illegal bytesequence
$ ENOMEDIUM
85 Intet medie fundet
$ EMEDIUMTYPE
86 Forkert medietype
$ EOVERFLOW
87 Værdi for stor til at gemmes i datatypen
$ ECANCELED
88 Operation aflyst
$ EIDRM
89 Identifikator fjernet
$ ENOMSG
90 Ingen besked af ønsket type
$ ENOTSUP
91 Ikke understøttet
$set 2
$ SIGHUP
1 Læg på
$ SIGINT
2 Afbryd
$ SIGQUIT
3 Slut
$ SIGILL
4 Ulovlig instruktion
$ SIGTRAP
5 Sporings- eller afbrydelsespunktfælde
$ SIGABRT
6 Abortfælde
$ SIGEMT
7 Emuleringsfælde
$ SIGFPE
8 Kommatalsundtagelse
$ SIGKILL
9 Dræbt
$ SIGBUS
10 Busfejl
$ SIGSEGV
11 Segmenteringsfejl
$ SIGSYS
12 Defekt systemkald
$ SIGPIPE
13 Afbrudt kommunikationskanal
$ SIGALRM
14 Alarmsignal
$ SIGTERM
15 Afsluttet
$ SIGURG
16 Hastende I/O-tilstand
$ SIGSTOP
17 Stoppet (signal)
$ SIGTSTP
18 Stoppet
$ SIGCONT
19 Fortsat
$ SIGCHLD
20 Barn afsluttet
$ SIGTTIN
21 Stoppet (terminalindlæsning)
$ SIGTTOU
22 Stoppet (terminaludskrivning)
$ SIGIO
23 I/O muligt
$ SIGXCPU
24 Processortidgrænse overskredet
$ SIGXFSZ
25 Filstørrelsesgrænse overskredet
$ SIGVTALRM
26 Virtuelt ur udløbet
$ SIGPROF
27 Profileringsur udløbet
$ SIGWINCH
28 Vinduesstørrelse ændret
$ SIGINFO
29 Informationsforespørgsel
$ SIGUSR1
30 Brugerdefineret signal 1
$ SIGUSR2
31 Brugerdefineret signal 2
