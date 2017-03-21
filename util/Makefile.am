## Process this file with automake to produce Makefile.in

SUBDIRS = .

noinst_LTLIBRARIES = libutil.la

AM_CPPFLAGS = -I$(top_srcdir) 		\
	$(VERSION_FLAGS) 		\
	-DG_LOG_DOMAIN=\"util\"		\
	$(GMIME_CFLAGS)			\
	$(GLIB_CFLAGS)

libutil_la_SOURCES =			\
	gtrie.c				\
	gtrie.h				\
	packed.c			\
	packed.h			\
	url-scanner.c			\
	url-scanner.h

