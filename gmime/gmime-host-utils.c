/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003-2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <errno.h>

#include "gmime-error.h"
#include "gmime-host-utils.h"


#if !defined (HAVE_GETHOSTBYNAME_R) || !defined (HAVE_GETHOSTBYADDR_R)
G_LOCK_DEFINE_STATIC (gethost_mutex);
#endif


#define ALIGN(x) (((x) + (sizeof (char *) - 1)) & ~(sizeof (char *) - 1))

#define GETHOST_PROCESS(h, host, buf, buflen) G_STMT_START {           \
	int num_aliases = 0, num_addrs = 0;                            \
	int req_length;                                                \
	char *p;                                                       \
	int i;                                                         \
	                                                               \
	/* check to make sure we have enough room in our buffer */     \
	req_length = 0;                                                \
	if (h->h_aliases) {                                            \
		for (i = 0; h->h_aliases[i]; i++)                      \
			req_length += strlen (h->h_aliases[i]) + 1;    \
		num_aliases = i;                                       \
	}                                                              \
	                                                               \
	if (h->h_addr_list) {                                          \
		for (i = 0; h->h_addr_list[i]; i++)                    \
			req_length += h->h_length;                     \
		num_addrs = i;                                         \
	}                                                              \
	                                                               \
	req_length += sizeof (char *) * (num_aliases + 1);             \
	req_length += sizeof (char *) * (num_addrs + 1);               \
	req_length += strlen (h->h_name) + 1;                          \
	                                                               \
	if (buflen < req_length) {                                     \
		G_UNLOCK (gethost_mutex);                              \
		return ERANGE;                                         \
	}                                                              \
	                                                               \
	/* we store the alias/addr pointers in the buffer */           \
	/* their addresses here. */                                    \
	p = buf;                                                       \
	if (num_aliases) {                                             \
		host->h_aliases = (char **) p;                         \
		p += sizeof (char *) * (num_aliases + 1);              \
	} else                                                         \
		host->h_aliases = NULL;                                \
                                                                       \
	if (num_addrs) {                                               \
		host->h_addr_list = (char **) p;                       \
		p += sizeof (char *) * (num_addrs + 1);                \
	} else                                                         \
		host->h_addr_list = NULL;                              \
	                                                               \
	/* copy the host name into the buffer */                       \
	host->h_name = p;                                              \
	strcpy (p, h->h_name);                                         \
	p += strlen (h->h_name) + 1;                                   \
	host->h_addrtype = h->h_addrtype;                              \
	host->h_length = h->h_length;                                  \
	                                                               \
	/* copy the aliases/addresses into the buffer */               \
        /* and assign pointers into the hostent */                     \
	*p = 0;                                                        \
	if (num_aliases) {                                             \
		for (i = 0; i < num_aliases; i++) {                    \
			strcpy (p, h->h_aliases[i]);                   \
			host->h_aliases[i] = p;                        \
			p += strlen (h->h_aliases[i]);                 \
		}                                                      \
		host->h_aliases[num_aliases] = NULL;                   \
	}                                                              \
	                                                               \
	if (num_addrs) {                                               \
		for (i = 0; i < num_addrs; i++) {                      \
			memcpy (p, h->h_addr_list[i], h->h_length);    \
			host->h_addr_list[i] = p;                      \
			p += h->h_length;                              \
		}                                                      \
		host->h_addr_list[num_addrs] = NULL;                   \
	}                                                              \
} G_STMT_END


#define IPv6_BUFLEN_MIN  (sizeof (char *) * 3)


/**
 * g_gethostbyname_r:
 * @name: the host to resolve
 * @host: a buffer pointing to a struct hostent to use for storage
 * @buf: a buffer to use for hostname storage
 * @buflen: the size of @buf
 * @err: a GError
 *
 * A reentrant implementation of gethostbyname()
 *
 * Return value: 0 on success, ERANGE if @buflen is too small,
 * "something else" otherwise -1 is returned and @err is set
 * appropriately.
 **/
int
g_gethostbyname_r (const char *name, struct hostent *host,
		   char *buf, size_t buflen, GError **err)
{
#ifdef ENABLE_IPv6
	struct addrinfo hints, *res;
	int retval, len;
	char *addr;
	
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = 0;
	hints.ai_protocol = 0;
	
	if ((retval = getaddrinfo (name, NULL, &hints, &res)) != 0) {
		g_set_error (err, GMIME_ERROR, retval, gai_strerror (retval));
		return -1;
	}
	
	len = ALIGN (strlen (res->ai_canonname) + 1);
	if (buflen < IPv6_BUFLEN_MIN + len + res->ai_addrlen + sizeof (char *))
		return ERANGE;
	
	/* h_name */
	strcpy (buf, res->ai_canonname);
	host->h_name = buf;
	buf += len;
	
	/* h_aliases */
	((char **) buf)[0] = NULL;
	host->h_aliases = (char **) buf;
	buf += sizeof (char *);
	
	/* h_addrtype and h_length */
	host->h_length = res->ai_addrlen;
	if (res->ai_family == PF_INET6) {
		host->h_addrtype = AF_INET6;
		
		addr = (char *) &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
	} else {
		host->h_addrtype = AF_INET;
		
		addr = (char *) &((struct sockaddr_in *) res->ai_addr)->sin_addr;
	}
	
	memcpy (buf, addr, host->h_length);
	addr = buf;
	buf += ALIGN (host->h_length);
	
	/* h_addr_list */
	((char **) buf)[0] = addr;
	((char **) buf)[1] = NULL;
	host->h_addr_list = (char **) buf;
	
	freeaddrinfo (res);
	
	return 0;
#else /* No support for IPv6 addresses */
#ifdef HAVE_GETHOSTBYNAME_R
#ifdef GETHOSTBYNAME_R_FIVE_ARGS
	int herr = 0;
	
	if (gethostbyname_r (name, host, buf, buflen, &herr) && errno == 0 && herr == 0)
		return 0;
	
	if (errno == ERANGE)
		return ERANGE;
	
	g_set_error (err, GMIME_ERROR, errno ? errno : herr, errno ? g_strerror (errno) : hstrerror (herr));
	
	return -1;
#else
	struct hostent *hp;
	int retval, herr;
	
	if ((retval = gethostbyname_r (name, host, buf, buflen, &hp, &herr)) == ERANGE)
		return ERANGE;
	
	if (hp == NULL) {
		g_set_error (err, GMIME_ERROR, herr, hstrerror (herr));
		return -1;
	}
	
	return 0;
#endif
#else /* No support for gethostbyname_r */
	struct hostent *h;
	
	G_LOCK (gethost_mutex);
	
	h = gethostbyname (name);
	
	if (!h) {
		g_set_error (err, GMIME_ERROR, h_errno, hstrerror (h_errno));
		G_UNLOCK (gethost_mutex);
		return -1;
	}
	
	GETHOST_PROCESS (h, host, buf, buflen);
	
	G_UNLOCK (gethost_mutex);
	
	return 0;
#endif /* HAVE_GETHOSTBYNAME_R */
#endif /* ENABLE_IPv6 */
}


/**
 * g_gethostbyaddr_r:
 * @addr: the addr to resolve
 * @addrlen: address length
 * @af: Address Family
 * @host: a buffer pointing to a struct hostent to use for storage
 * @buf: a buffer to use for hostname storage
 * @buflen: the size of @buf
 * @err: a GError
 *
 * A reentrant implementation of gethostbyaddr()
 *
 * Return value: 0 on success, ERANGE if @buflen is too small,
 * "something else" otherwise -1 is returned and @err is set
 * appropriately.
 **/
int
g_gethostbyaddr_r (const char *addr, int addrlen, int af,
		   struct hostent *host, char *buf,
		   size_t buflen, GError **err)
{
#ifdef ENABLE_IPv6
	int retval, len;
	
	if ((retval = getnameinfo ((const struct sockaddr *) addr, addrlen, buf, buflen, NULL, 0, NI_NAMEREQD)) != 0) {
		g_set_error (err, GMIME_ERROR, retval, gai_strerror (retval));
		return -1;
	}
	
	len = ALIGN (strlen (buf) + 1);
	if (buflen < IPv6_BUFLEN_MIN + len + addrlen + sizeof (char *))
		return ERANGE;
	
	/* h_name */
	host->h_name = buf;
	buf += len;
	
	/* h_aliases */
	((char **) buf)[0] = NULL;
	host->h_aliases = (char **) buf;
	buf += sizeof (char *);
	
	/* h_addrtype and h_length */
	host->h_length = addrlen;
	host->h_addrtype = af;
	
	memcpy (buf, addr, host->h_length);
	addr = buf;
	buf += ALIGN (host->h_length);
	
	/* h_addr_list */
	((char **) buf)[0] = addr;
	((char **) buf)[1] = NULL;
	host->h_addr_list = (char **) buf;
	
	return 0;
#else /* No support for IPv6 addresses */
#ifdef HAVE_GETHOSTBYADDR_R
#ifdef GETHOSTBYADDR_R_SEVEN_ARGS
	int herr = 0;
	
	if (gethostbyaddr_r (addr, addrlen, af, host, buf, buflen, &herr) && errno == 0 && herr == 0)
		return 0;
	
	if (errno == ERANGE)
		return ERANGE;
	
	g_set_error (err, GMIME_ERROR, errno ? errno : herr, errno ? g_strerror (errno) : hstrerror (herr));
	
	return -1;
#else
	struct hostent *hp;
	int retval, herr;
	
	if ((retval = gethostbyaddr_r (addr, addrlen, af, host, buf, buflen, &hp, &herr)) == ERANGE)
		return ERANGE;
	
	if (hp == NULL) {
		g_set_error (err, GMIME_ERROR, herr, hstrerror (herr));
		return -1;
	}
	
	return 0;
#endif
#else /* No support for gethostbyaddr_r */
	struct hostent *h;
	
	G_LOCK (gethost_mutex);
	
	h = gethostbyaddr (addr, addrlen, af);
	
	if (!h) {
		g_set_error (err, GMIME_ERROR, h_errno, hstrerror (h_errno));
		G_UNLOCK (gethost_mutex);
		return -1;
	}
	
	GETHOST_PROCESS (h, host, buf, buflen);
	
	G_UNLOCK (gethost_mutex);
	
	return 0;
#endif /* HAVE_GETHOSTBYADDR_R */
#endif /* ENABLE_IPv6 */
}
