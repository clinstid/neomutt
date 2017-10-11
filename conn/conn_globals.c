#include "config.h"

#ifdef USE_SSL
const char *CertificateFile;
const char *SslClientCert;
const char *EntropyFile;
const char *SslCiphers;
#ifdef USE_SSL_GNUTLS
short SslMinDhPrimeBits;
const char *SslCaCertificatesFile;
#endif
#endif

short ConnectTimeout;

#ifdef USE_SOCKET
const char *Preconnect;
const char *Tunnel;
#endif /* USE_SOCKET */
