$set 1
$ EPERM
1 Bewerking niet toegestaan
$ ENOENT
2 Bestand of directory niet gevonden
$ ESRCH
3 Proces bestaat niet
$ EINTR
4 Onderbroken systeemaanroep
$ EIO
5 Invoer/uitvoer-fout
$ ENXIO
6 Apparaat niet geconfigureerd
$ E2BIG
7 Te veel of te lange argumenten
$ ENOEXEC
8 Ongeldige bestandsindeling voor uitvoering
$ EBADF
9 Ongeldige bestandsverwijzing
$ ECHILD
10 Proces heeft geen kinderen
$ EDEADLK
11 "Deadlock" in systeem voorkomen
$ ENOMEM
12 Kan geen geheugen verkrijgen
$ EACCES
13 Toegang geweigerd
$ EFAULT
14 Ongeldig geheugenadres
$ ENOTBLK
15 Apparaat niet per blok addresseerbaar
$ EBUSY
16 Apparaat bezig
$ EEXIST
17 Bestandsnaam in gebruik
$ EXDEV
18 Koppeling kruist twee bestandssystemen
$ ENODEV
19 Bewerking niet ondersteund door apparaat
$ ENOTDIR
20 Geen directory
$ EISDIR
21 Is een directory
$ EINVAL
22 Ongeldig argument
$ ENFILE
23 Te veel open bestanden in het systeem
$ EMFILE
24 Te veel open bestanden
$ ENOTTY
25 ioctl niet ondersteund door apparaat
$ ETXTBSY
26 Programmabestand in gebruik
$ EFBIG
27 Bestand te groot
$ ENOSPC
28 Bestandssysteem vol
$ ESPIPE
29 Ongeldige zoekopdracht
$ EROFS
30 Alleen leestoegang tot bestandssysteem
$ EMLINK
31 Te veel koppelingen
$ EPIPE
32 Gebroken pijp
$ EDOM
33 Numeriek argument buiten domein
$ ERANGE
34 Numeriek resultaat buiten bereik
$ EAGAIN, EWOULDBLOCK
35 Hulpmiddel tijdelijk niet beschikbaar
$ EINPROGRESS
36 Bewerking in gang gezet
$ EALREADY
37 Bewerking reeds aan de gang
$ ENOTSOCK
38 Bewerking alleen toepasbaar op socket
$ EDESTADDRREQ
39 Bestemmingsadres vereist
$ EMSGSIZE
40 Boodschap te lang
$ EPROTOTYPE
41 Onjuist protocoltype voor socket
$ ENOPROTOOPT
42 Protocol niet beschikbaar
$ EPROTONOSUPPORT
43 Protocol niet ondersteund
$ ESOCKTNOSUPPORT
44 Sockettype niet ondersteund
$ EOPNOTSUPP
45 Bewerking niet ondersteund
$ EPFNOSUPPORT
46 Protocolfamilie niet ondersteund
$ EAFNOSUPPORT
47 Adresfamilie niet ondersteund door protocolfamilie
$ EADDRINUSE
48 Adres al in gebruik
$ EADDRNOTAVAIL
49 Gewenst adres kan niet worden toegekend
$ ENETDOWN
50 Netwerk onbruikbaar
$ ENETUNREACH
51 Netwerk niet bereikbaar
$ ENETRESET
52 Netwerk verbrak verbinding na herstart
$ ECONNABORTED
53 Verbinding verbroken door applicatie
$ ECONNRESET
54 Verbinding verbroken door tegenpartij
$ ENOBUFS
55 Geen bufferruimte beschikbaar
$ EISCONN
56 Socket reeds verbonden
$ ENOTCONN
57 Socket is niet verbonden
$ ESHUTDOWN
58 Kan geen data verzenden via gesloten socket
$ ETOOMANYREFS
59 Te veel verwijzingen: splitsen niet mogelijk
$ ETIMEDOUT
60 Verbinding verbroken wegens inactiviteit
$ ECONNREFUSED
61 Verbinding geweigerd
$ ELOOP
62 Te veel niveaus van symbolische verwijzingen
$ ENAMETOOLONG
63 Bestandsnaam te lang
$ EHOSTDOWN
64 Tegenpartij vertoont geen netwerkactiviteit
$ EHOSTUNREACH
65 Tegenpartij onbereikbaar
$ ENOTEMPTY
66 Directory niet leeg
$ EPROCLIM
67 Te veel processen
$ EUSERS
68 Te veel gebruikers
$ EDQUOT
69 Diskquotum overschreden
$ ESTALE
70 Bestandsverwijzing op NFS ongeldig geworden
$ EREMOTE
71 Te veel verwijzingen in dit pad
$ EBADRPC
72 RPC-structuur misvormd
$ ERPCMISMATCH
73 Verkeerde RPC-versie
$ EPROGUNAVAIL
74 RPC-programma niet beschikbaar
$ EPROGMISMATCH
75 Verkeerde programmaversie
$ EPROCUNAVAIL
76 Verkeerde procedure voor programma
$ ENOLCK
77 Geen locks beschikbaar
$ ENOSYS
78 Systeemfunctie niet geimplementeerd
$ EFTYPE
79 Bestandstype of -indeling ongeldig
$ EAUTH
80 Authenticatie fout
$ ENEEDAUTH
81 Authenticatie procedure benodigd
$ EIPSEC
82 IPsec verwerkingsfout
$ ENOATTR
83 Attribuut niet gevonden
$ EILSEQ
84 Ongeldige coderings reeks
$ ENOMEDIUM
85 Medium niet gevonden
$ EMEDIUMTYPE
86 Verkeerd medium type
$ EOVERFLOW
87 Waarde te groot om te bewaren in data type
$ ECANCELED
88 Bewerking afgebroken
$ EIDRM
89 Identificeerder verwijderd
$ ENOMSG
90 Geen boodschap van het gewenste type
$ ENOTSUP
91 Niet ondersteund
$set 2
$ SIGHUP
1 Opgehangen
$ SIGINT
2 Onderbroken
$ SIGQUIT
3 Opgegeven
$ SIGILL
4 Ongeldige instructie
$ SIGTRAP
5 Tracing/BPT-onderbreking
$ SIGABRT
6 Afgebroken
$ SIGEMT
7 Emulatiefout
$ SIGFPE
8 Fout bij deling
$ SIGKILL
9 Gedood
$ SIGBUS
10 Busfout
$ SIGSEGV
11 Segmentatiefout
$ SIGSYS
12 Verkeerde systeemaanroep
$ SIGPIPE
13 Gebroken pijp
$ SIGALRM
14 Alarmsignaal
$ SIGTERM
15 Beeindigd
$ SIGURG
16 Invoer/uitvoer dringend
$ SIGSTOP
17 Gestopt (signaal)
$ SIGTSTP
18 Gestopt
$ SIGCONT
19 Voortgezet
$ SIGCHLD
20 Kindproces geeindigd
$ SIGTTIN
21 Gestopt (invoer van terminal)
$ SIGTTOU
22 Gestopt (uitvoer naar terminal)
$ SIGIO
23 Invoer/uitvoer mogelijk
$ SIGXCPU
24 Limiet op CPU-tijd overschreden
$ SIGXFSZ
25 Maximale bestandsgrootte overschreden
$ SIGVTALRM
26 Alarmsignaal van virtuele wekker
$ SIGPROF
27 Alarmsignaal van profiling-wekker
$ SIGWINCH
28 Vensterafmeting gewijzigd
$ SIGINFO
29 Verzoek om informatie
$ SIGUSR1
30 Gebruikersignaal 1
$ SIGUSR2
31 Gebruikersignaal 2
