## Process this file with automake to produce Makefile.in

SUBDIRS = .

noinst_LTLIBRARIES = libutil.la

AM_CPPFLAGS = -I$(top_srcdir) 		\
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
	packed.c			\
	packed.h			\
	url-scanner.c			\
	url-scanner.h

