# define OPENSSL_NO_CAMELLIA
# define OPENSSL_NO_EC_NISTP_64_GCC_128
# define OPENSSL_NO_CMS
# define OPENSSL_NO_COMP
# define OPENSSL_NO_GMP
# define OPENSSL_NO_GOST
# define OPENSSL_NO_JPAKE
# define OPENSSL_NO_KRB5
# define OPENSSL_NO_MD2
# define OPENSSL_NO_PSK
# define OPENSSL_NO_RC5
# define OPENSSL_NO_RFC3779
# define OPENSSL_NO_SCTP
# define OPENSSL_NO_SEED
# define OPENSSL_NO_SRP
# define OPENSSL_NO_SSL2
# define OPENSSL_NO_STORE

# define OPENSSL_THREADS
# define OPENSSL_NO_DYNAMIC_ENGINE

/* The OPENSSL_NO_* macros are also defined as NO_* if the application
   asks for it.  This is a transient feature that is provided for those
   who haven't had the time to do the appropriate changes in their
   applications.  */
#ifdef OPENSSL_ALGORITHM_DEFINES
# if defined(OPENSSL_NO_CAMELLIA) && !defined(NO_CAMELLIA)
#  define NO_CAMELLIA
# endif
# if defined(OPENSSL_NO_EC_NISTP_64_GCC_128) && !defined(NO_EC_NISTP_64_GCC_128)
#  define NO_EC_NISTP_64_GCC_128
# endif
# if defined(OPENSSL_NO_CMS) && !defined(NO_CMS)
#  define NO_CMS
# endif
# if defined(OPENSSL_NO_COMP) && !defined(NO_COMP)
#  define NO_COMP
# endif
# if defined(OPENSSL_NO_GMP) && !defined(NO_GMP)
#  define NO_GMP
# endif
# if defined(OPENSSL_NO_GOST) && !defined(NO_GOST)
#  define NO_GOST
# endif
# if defined(OPENSSL_NO_JPAKE) && !defined(NO_JPAKE)
#  define NO_JPAKE
# endif
# if defined(OPENSSL_NO_KRB5) && !defined(NO_KRB5)
#  define NO_KRB5
# endif
# if defined(OPENSSL_NO_MD2) && !defined(NO_MD2)
#  define NO_MD2
# endif
# if defined(OPENSSL_NO_RC5) && !defined(NO_RC5)
#  define NO_RC5
# endif
# if defined(OPENSSL_NO_RFC3779) && !defined(NO_RFC3779)
#  define NO_RFC3779
# endif
# if defined(OPENSSL_NO_SCTP) && !defined(NO_SCTP)
#  define NO_SCTP
# endif
# if defined(OPENSSL_NO_SEED) && !defined(NO_SEED)
#  define NO_SEED
# endif
# if defined(OPENSSL_NO_SRP) && !defined(NO_SRP)
#  define NO_SRP
# endif
# if defined(OPENSSL_NO_SSL2) && !defined(NO_SSL2)
#  define NO_SSL2
# endif
# if defined(OPENSSL_NO_STORE) && !defined(NO_STORE)
#  define NO_STORE
# endif
#endif

