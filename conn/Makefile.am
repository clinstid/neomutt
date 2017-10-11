## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = account.h connection.h sasl.h sasl_plain.h socket.h ssl.h tunnel.h

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libconn.a

libconn_a_SOURCES = getdomain.c sasl_plain.c socket.c tunnel.c

if USE_SASL
libconn_a_SOURCES += sasl.o
endif
if USE_SSL_OPENSSL
libconn_a_SOURCES += ssl.o
endif
if USE_SSL_GNUTLS
libconn_a_SOURCES += ssl_gnutls.o
endif

