$set 1
$ EPERM
1 Operation nicht erlaubt
$ ENOENT
2 Datei oder Verzeichnis nicht gefunden
$ ESRCH
3 Prozess nicht gefunden
$ EINTR
4 Unterbrochener Systemaufruf
$ EIO
5 Ein/Ausgabefehler
$ ENXIO
6 Gerät ist nicht konfiguriert
$ E2BIG
7 Argumentliste ist zu lang
$ ENOEXEC
8 Unbekanntes ausführbares Format
$ EBADF
9 Ungültiger Dateideskriptor
$ ECHILD
10 Kein Kindprozess
$ EDEADLK
11 Ein "Deadlock" wurde vermieden
$ ENOMEM
12 Kann nicht genug Speicher belegen
$ EACCES
13 Zugriff verboten
$ EFAULT
14 Ungültige Adresse
$ ENOTBLK
15 Blockgerät wird benötigt
$ EBUSY
16 Gerät ist belegt
$ EEXIST
17 Datei existiert bereits
$ EXDEV
18 Link zwischen verschiedenen Geräten
$ ENODEV
19 Die Operation wird von diesem Gerät nicht unterstützt
$ ENOTDIR
20 Kein Verzeichnis
$ EISDIR
21 Ist ein Verzeichnis
$ EINVAL
22 Ungültiges Argument
$ ENFILE
23 Zu viele offene Dateien im gesamten System
$ EMFILE
24 Zu viele offene Dateien
$ ENOTTY
25 Ungültiger ioctl für dieses Gerät
$ ETXTBSY
26 Ausführbare Datei wird benutzt
$ EFBIG
27 Datei zu groß
$ ENOSPC
28 Kein Platz mehr auf dem Gerät
$ ESPIPE
29 Ungültige Positionierung
$ EROFS
30 Dateisystem ist schreibgeschützt
$ EMLINK
31 Zu viele Links
$ EPIPE
32 Unterbrochene pipe
$ EDOM
33 Numerisches Argument ausserhalb des Wertebereichs
$ ERANGE
34 Ergebnis zu groß
$ EAGAIN, EWOULDBLOCK
35 Resource vorübergehend nicht verfügbar
$ EINPROGRESS
36 Operation wird jetzt fortgeführt
$ EALREADY
37 Operation wird bereits ausgeführt
$ ENOTSOCK
38 Deskriptor ist kein Socket
$ EDESTADDRREQ
39 Zieladresse benötigt
$ EMSGSIZE
40 Nachricht zu lang
$ EPROTOTYPE
41 Ungültiger Protokolltyp für diesen Socket
$ ENOPROTOOPT
42 Protokoll nicht verfügbar
$ EPROTONOSUPPORT
43 Protokoll nicht unterstützt
$ ESOCKTNOSUPPORT
44 Sockettyp nicht unterstützt
$ EOPNOTSUPP
45 Operation nicht unterstützt
$ EPFNOSUPPORT
46 Protokollfamilie nicht unterstützt
$ EAFNOSUPPORT
47 Addressart wird von der Protokollfamilie nicht unterstützt
$ EADDRINUSE
48 Addresse wird bereits benutzt
$ EADDRNOTAVAIL
49 Kann angeforderte Adresse nicht belegen
$ ENETDOWN
50 Netzwerk nicht verfügbar
$ ENETUNREACH
51 Netzwerk nicht erreichbar
$ ENETRESET
52 Netzwerk hat Verbindung mit Reset abgebrochen
$ ECONNABORTED
53 Software verursachte einen Verbindungsabbruch
$ ECONNRESET
54 Verbindung wurde von der Gegenstelle geschlossen
$ ENOBUFS
55 Keine Buffer verfügbar
$ EISCONN
56 Socket ist schon verbunden
$ ENOTCONN
57 Socket ist nicht verbunden
$ ESHUTDOWN
58 Kann nach einem Socket-Shutdown nicht mehr senden
$ ETOOMANYREFS
59 Zu viele Referenzen, kann nicht verbinden
$ ETIMEDOUT
60 Verbindungsabbruch durch Zeitüberschreitung
$ ECONNREFUSED
61 Verbindung wurde verweigert
$ ELOOP
62 Zu viele Ebenen von symbolischen Links
$ ENAMETOOLONG
63 Dateiname zu lang
$ EHOSTDOWN
64 Host nicht verfügbar
$ EHOSTUNREACH
65 Keine Route zum Host
$ ENOTEMPTY
66 Verzeichnis ist nicht leer
$ EPROCLIM
67 Zu viele Prozesse
$ EUSERS
68 Zu viele Benutzer
$ EDQUOT
69 Plattenplatzlimit erschöpft
$ ESTALE
70 Verwaister NFS-Dateideskriptor
$ EREMOTE
71 Zu viele Fernverweise in diesem Zugriff
$ EBADRPC
72 RPC-Struktur ist ungültig
$ ERPCMISMATCH
73 RPC-Version stimmt nicht
$ EPROGUNAVAIL
74 RPC-Programm nicht verfügbar
$ EPROGMISMATCH
75 Falsche Programmversion
$ EPROCUNAVAIL
76 Falsche Prozedur für dieses Programm
$ ENOLCK
77 Keine Dateisperren verfügbar
$ ENOSYS
78 Funktion nicht implementiert
$ EFTYPE
79 Ungültiger Dateityp oder Dateiformat
$ EAUTH
80 Authentikationsfehler
$ ENEEDAUTH
81 Authentikator benötigt
$ EIPSEC
82 IPsec Verarbeitungsfehler
$ ENOATTR
83 Attribut nicht gefunden
$ EILSEQ
84 Illegale Byte-Sequenz
$ ENOMEDIUM
85 Kein Medium gefunden
$ EMEDIUMTYPE
86 Falscher Medientyp
$ EOVERFLOW
87 Wert zu groß, um in Datentyp zu speichern 
$ ECANCELED
88 Operation abgebrochen
$ EIDRM
89 Identifizierung entfernt
$ ENOMSG
90 Keine Nachricht vom gewünschten Typ
$ ENOTSUP
91 Nicht unterstützt
$set 2
$ SIGHUP
1 Verbindungsende
$ SIGINT
2 Unterbrechung
$ SIGQUIT
3 Ende
$ SIGILL
4 Ungültiger Maschinenbefehl
$ SIGTRAP
5 Trace/BPT trap
$ SIGABRT
6 Abort trap
$ SIGEMT
7 EMT trap
$ SIGFPE
8 Fließkommafehler
$ SIGKILL
9 Killed
$ SIGBUS
10 Bus Fehler
$ SIGSEGV
11 Illegaler Speicherzugriff
$ SIGSYS
12 Ungültiger Systemaufruf
$ SIGPIPE
13 Unterbrochene Pipe
$ SIGALRM
14 Wecker
$ SIGTERM
15 Beendet
$ SIGURG
16 Dringende Ein/Ausgabeanforderung
$ SIGSTOP
17 Angehalten (Signal)
$ SIGTSTP
18 Angehalten
$ SIGCONT
19 Fortgesetzt
$ SIGCHLD
20 Kindprozess beendet
$ SIGTTIN
21 Angehalten (tty Eingabe)
$ SIGTTOU
22 Angehalten (tty Ausgabe)
$ SIGIO
23 Ein/Ausgabe ist möglich
$ SIGXCPU
24 CPU-Zeitlimit erschöpft
$ SIGXFSZ
25 Dateigröße hat das Limit erreicht
$ SIGVTALRM
26 Virtueller Zähler abgelaufen
$ SIGPROF
27 Profil-Zähler abgelaufen
$ SIGWINCH
28 Fenstergröße hat sich geändert
$ SIGINFO
29 Informationsanforderung
$ SIGUSR1
30 Benutzerdefiniertes Signal 1
$ SIGUSR2
31 Benutzerdefiniertes Signal 2
