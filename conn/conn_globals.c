/**
 * @file
 * Connection Global Variables
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page conn_globals Connection Global Variables
 *
 * Connection Global Variables
 */

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
