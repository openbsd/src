$set 1
$ EPERM
1 Operación no permitida
$ ENOENT
2 Fichero o directorio no existente
$ ESRCH
3 Proceso inexistente
$ EINTR
4 Llamada del sistema interrumpida
$ EIO
5 Error de E/S
$ ENXIO
6 Dispositivo no configurado
$ E2BIG
7 La lista de argumentos es demasiado amplia
$ ENOEXEC
8 Error en el formato del ejecutable 
$ EBADF
9 Descriptor de fichero incorrecto
$ ECHILD
10 No hay procesos hijo 
$ EDEADLK
11 Se ha evitado bloqueo del recurso
$ ENOMEM
12 No se pudo asignar la memoria pedida
$ EACCES
13 Permiso denegado
$ EFAULT
14 Dirección incorrecta
$ ENOTBLK
15 Se necesita un dispositivo de bloques
$ EBUSY
16 Dispositivo ocupado
$ EEXIST
17 El fichero ya existe
$ EXDEV
18 Enlace entre dispositivos
$ ENODEV
19 Operación inadecuada para este dispositivo
$ ENOTDIR
20 No es un directorio
$ EISDIR
21 Es un directorio
$ EINVAL
22 Argumento inadecuado
$ ENFILE
23 Hay demasiados ficheros abiertos en el sistema
$ EMFILE
24 Hay demasiados ficheros abiertos
$ ENOTTY
25 IOCTL inapropiado para el dispositivo
$ ETXTBSY
26 Fichero de texto ocupado
$ EFBIG
27 Fichero demasiado grande
$ ENOSPC
28 No queda espacio libre en el dispositivo
$ ESPIPE
29 Búsqueda ilegal
$ EROFS
30 Fichero del sistema de sólo lectura
$ EMLINK
31 Demasiados enlaces
$ EPIPE
32 Canal (pipe) roto
$ EDOM
33 El argumento numérico está fuera de rango
$ ERANGE
34 El resultado es demasiado grande
$ EAGAIN, EWOULDBLOCK
35 El recurso está temporalmente no disponible
$ EINPROGRESS
36 Operación actualmente en proceso
$ EALREADY
37 La operación ya estaba realizándose
$ ENOTSOCK
38 Se ha intentado realizar una operación de socket en algo que no es un socket
$ EDESTADDRREQ
39 Se necesita una dirección de destino
$ EMSGSIZE
40 Mensaje demasiado largo
$ EPROTOTYPE
41 Tipo erróneo de protocolo para el socket
$ ENOPROTOOPT
42 Protocolo no disponible
$ EPROTONOSUPPORT
43 Protocolo no contemplado
$ ESOCKTNOSUPPORT
44 Tipo de socket no contemplado
$ EOPNOTSUPP
45 Operación no contemplada
$ EPFNOSUPPORT
46 Familia de protocolos no contemplada
$ EAFNOSUPPORT
47 Familia de direcciones no contemplada por la familia de protocolos
$ EADDRINUSE
48 La dirección ya está en uso
$ EADDRNOTAVAIL
49 No se pudo asignar la dirección pedida
$ ENETDOWN
50 La red no funciona
$ ENETUNREACH
51 No se puede acceder a la red
$ ENETRESET
52 La conexión a la red se interrumpió al reinicializar
$ ECONNABORTED
53 La conexi@@@n se ha abortado debid a problemas en el software
$ ECONNRESET
54 El interlocutor ha reinicializado la comunicación
$ ENOBUFS
55 No queda espacio en el almacenamiento intermedio
$ EISCONN
56 El socket ya estaba conectado
$ ENOTCONN
57 El socket no está conectado
$ ESHUTDOWN
58 No se puede enviar tras la desconexión del socket
$ ETOOMANYREFS
59 Demasiadas referencias: no se puede unir
$ ETIMEDOUT
60 El tiempo de conexión ha expirado
$ ECONNREFUSED
61 Conexión rehusada
$ ELOOP
62 Demasiados niveles de enlaces simbólicos
$ ENAMETOOLONG
63 Nombre de fichero demasiado largo
$ EHOSTDOWN
64 La máquina está caída
$ EHOSTUNREACH
65 No hay ruta hasta la máquina
$ ENOTEMPTY
66 Directorio no vacío
$ EPROCLIM
67 Demasiados procesos
$ EUSERS
68 Demasiados usuarios
$ EDQUOT
69 Cuota de disco sobrepasada
$ ESTALE
70 Manejador de fichero NFS inválido
$ EREMOTE
71 Camino con demasiados niveles
$ EBADRPC
72 La estructura de la RPC es errónea
$ ERPCMISMATCH
73 La versión de la RPC es errónea
$ EPROGUNAVAIL
74 La RPC no está accesible
$ EPROGMISMATCH
75 Versión errónea del programa
$ EPROCUNAVAIL
76 Procedimiento erróneo para el programa
$ ENOLCK
77 No hay cerrojos disponibles
$ ENOSYS
78 Función no realizada
$ EFTYPE
79 Tipo de fichero o formato inapropiado
$set 2
$ SIGHUP
1 Bloqueo del sistema
$ SIGINT
2 Interrupción
$ SIGQUIT
3 Terminación
$ SIGILL
4 Instrucción ilegal
$ SIGTRAP
5 Depuración
$ SIGABRT
6 Llamada de aborto
$ SIGEMT
7 EMT trap
$ SIGFPE
8 Excepción de coma flotante
$ SIGKILL
9 Asesinato
$ SIGBUS
10 Error en el bus
$ SIGSEGV
11 Fallo de segmentación
$ SIGSYS
12 Llamada al sistema errónea
$ SIGPIPE
13 Canal (pipe) roto
$ SIGALRM
14 Alarma del reloj
$ SIGTERM
15 Terminado
$ SIGURG
16 Condición urgente de E/S
$ SIGSTOP
17 Detenido (señal)
$ SIGTSTP
18 Detenido
$ SIGCONT
19 Continuado {continuando}
$ SIGCHLD
20 Proceso hijo finalizado
$ SIGTTIN
21 Detenido (entrada tty)
$ SIGTTOU
22 Detenido (salida tty)
$ SIGIO
23 E/S posible {permisible}
$ SIGXCPU
24 Se ha sobrepasado el tiempo límite de la CPU
$ SIGXFSZ
25 Se ha sobrepasado el límite de longitud de fichero
$ SIGVTALRM
26 Temporizador virtual expirado
$ SIGPROF
27 Perfilador expirado
$ SIGWINCH
28 Cambios en el tamaño de la ventana
$ SIGINFO
29 Petición de información
$ SIGUSR1
30 Señal definida por el usuario nº 1
$ SIGUSR2
31 Señal definida por el usuario nº 2
