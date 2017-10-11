#ifndef _CONN_GLOBALS_H
#define _CONN_GLOBALS_H

#ifdef USE_SSL
extern const char *CertificateFile;
extern const char *SslClientCert;
extern const char *EntropyFile;
extern const char *SslCiphers;
#ifdef USE_SSL_GNUTLS
extern short SslMinDhPrimeBits;
extern const char *SslCaCertificatesFile;
#endif
#endif

extern short ConnectTimeout;

#ifdef USE_SOCKET
extern const char *Preconnect;
extern const char *Tunnel;
#endif

#endif /* _CONN_GLOBALS_H */
