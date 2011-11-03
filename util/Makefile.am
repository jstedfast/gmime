## Process this file with automake to produce Makefile.in

SUBDIRS = .

noinst_LTLIBRARIES = libutil.la

INCLUDES = -I$(top_srcdir) 		\
	$(VERSION_FLAGS) 		\
	-DG_LOG_DOMAIN=\"util\"		\
	$(GMIME_CFLAGS)			\
	$(GLIB_CFLAGS)

libutil_la_SOURCES =			\
	cache.c				\
	cache.h				\
	gtrie.c				\
	gtrie.h				\
	list.c				\
	list.h				\
	md5-utils.c			\
	md5-utils.h			\
	url-scanner.c			\
	url-scanner.h

