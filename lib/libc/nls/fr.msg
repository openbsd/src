$set 1
$ EPERM
1 Opération non autorisée
$ ENOENT
2 Fichier ou répertoire introuvable
$ ESRCH
3 Processus introuvable
$ EINTR
4 Appel système interrompu
$ EIO
5 Erreur d'entrée/sortie
$ ENXIO
6 Périphérique non configuré
$ E2BIG
7 Liste de paramètres trop longue
$ ENOEXEC
8 Erreur de format d'exécutable
$ EBADF
9 Descripteur de fichier invalide
$ ECHILD
10 Pas de processus fils
$ EDEADLK
11 Etreinte fatale évitée
$ ENOMEM
12 Plus de mémoire
$ EACCES
13 Autorisation refusée
$ EFAULT
14 Adresse invalide
$ ENOTBLK
15 Nécessite un périphérique en mode bloc
$ EBUSY
16 Périphérique occupé
$ EEXIST
17 Fichier existant
$ EXDEV
18 Lien hors du périphérique
$ ENODEV
19 Opération non supportée par le périphérique
$ ENOTDIR
20 N'est pas un répertoire
$ EISDIR
21 Est un répertoire
$ EINVAL
22 Paramètre invalide
$ ENFILE
23 Trop de fichiers ouverts dans le système
$ EMFILE
24 Trop de fichiers ouverts
$ ENOTTY
25 Ioctl inconnu du périphérique
$ ETXTBSY
26 Fichier exécutable utilisé
$ EFBIG
27 Fichier trop grand
$ ENOSPC
28 Plus de place sur le périphérique
$ ESPIPE
29 Positionnement illégal
$ EROFS
30 Système de fichier protégé en écriture
$ EMLINK
31 Trop de liens
$ EPIPE
32 Tube cassé
$ EDOM
33 Paramètre numérique hors du domaine
$ ERANGE
34 Résultat trop grand
$ EAGAIN, EWOULDBLOCK
35 Ressource temporairement indisponible
$ EINPROGRESS
36 Opération actuellement en cours
$ EALREADY
37 Opération déjà en cours
$ ENOTSOCK
38 Opération réservée aux sockets
$ EDESTADDRREQ
39 Adresse de destination nécessaire
$ EMSGSIZE
40 Message trop long
$ EPROTOTYPE
41 Protocole inadapté au socket
$ ENOPROTOOPT
42 Protocole non disponible
$ EPROTONOSUPPORT
43 Protocole non supporté
$ ESOCKTNOSUPPORT
44 Type de socket non supporté
$ EOPNOTSUPP
45 Opération non supportée
$ EPFNOSUPPORT
46 Famille de protocoles non supportée
$ EAFNOSUPPORT
47 Famille d'adresses non supportée par cette famille de protocoles
$ EADDRINUSE
48 Adresse déjà utilisée
$ EADDRNOTAVAIL
49 Impossible d'affecter l'adresse demandée
$ ENETDOWN
50 Réseau arrêté
$ ENETUNREACH
51 Réseau non atteignable
$ ENETRESET
52 Connexion perdue après RAZ du réseau
$ ECONNABORTED
53 Fin de connexion causée par logiciel
$ ECONNRESET
54 Connexion terminée par le correspondant
$ ENOBUFS
55 Plus de place pour la mémoire tampon
$ EISCONN
56 Socket déjà connecté
$ ENOTCONN
57 Socket non connecté
$ ESHUTDOWN
58 Impossible de transmettre après fermeture du socket
$ ETOOMANYREFS
59 Trop de références: impossible à réassembler
$ ETIMEDOUT
60 Délai de connexion expiré
$ ECONNREFUSED
61 Connexion refusée
$ ELOOP
62 Trop de niveaux de liens symboliques
$ ENAMETOOLONG
63 Nom de fichier trop long
$ EHOSTDOWN
64 Machine arrêtée
$ EHOSTUNREACH
65 Pas de route vers cette machine
$ ENOTEMPTY
66 Répertoire non vide
$ EPROCLIM
67 Trop de processus
$ EUSERS
68 Trop d'utilisateurs
$ EDQUOT
69 Quota disque épuisé
$ ESTALE
70 Identificateur NFS périmé
$ EREMOTE
71 Trop de points de montages dans le chemin
$ EBADRPC
72 Structure RPC invalide
$ ERPCMISMATCH
73 Mauvaise version RPC
$ EPROGUNAVAIL
74 Programme RPC non disponible
$ EPROGMISMATCH
75 Mauvaise version de programme
$ EPROCUNAVAIL
76 Procédure inexistante
$ ENOLCK
77 Pas de verrous disponibles
$ ENOSYS
78 Fonction non implémentée
$ EFTYPE
79 Type ou format de fichier inadapté
$set 2
$ SIGHUP
1 Raccroché
$ SIGINT
2 Interruption
$ SIGQUIT
3 Quitte
$ SIGILL
4 Instruction illégale
$ SIGTRAP
5 Point de trace/arrêt
$ SIGABRT
6 Avorté
$ SIGEMT
7 Appel émulateur
$ SIGFPE
8 Exception numérique
$ SIGKILL
9 Tué
$ SIGBUS
10 Erreur bus
$ SIGSEGV
11 Erreur de segmentation
$ SIGSYS
12 Appel système invalide
$ SIGPIPE
13 Tube cassé
$ SIGALRM
14 Alarme
$ SIGTERM
15 Terminé
$ SIGURG
16 Condition d'E/S urgente
$ SIGSTOP
17 Arrêté par un signal
$ SIGTSTP
18 Arrêté
$ SIGCONT
19 Reprise
$ SIGCHLD
20 Fin d'un fils
$ SIGTTIN
21 Arrêté (lecture sur tty)
$ SIGTTOU
22 Arrêté (écriture sur tty)
$ SIGIO
23 E/S possible
$ SIGXCPU
24 Limite du temps CPU atteinte
$ SIGXFSZ
25 Limite de taille de fichier atteinte
$ SIGVTALRM
26 Timer virtuel expiré
$ SIGPROF
27 Timer de profiling expiré
$ SIGWINCH
28 Changement de taille de la fenêtre
$ SIGINFO
29 Demande d'informations
$ SIGUSR1
30 Signal utilisateur 1
$ SIGUSR2
31 Signal utilisateur 2
