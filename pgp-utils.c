/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Nathan Thompson-Amato <ndt@jps.net>
 *           Dan Winship <danw@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright (C) Helix Code, Inc. (www.helixcode.com)
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pgp-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define d(x)
#define _(x) x

static char *pgp_path = NULL;
static PgpType pgp_type = PGP_TYPE_NONE;
static PgpPasswdFunc pgp_passwd_func = NULL;
static gpointer pgp_data = NULL;


static char *
pgp_get_passphrase (const char *userid)
{
	char *passphrase, *prompt, *type;
	
	switch (pgp_type) {
	case PGP_TYPE_GPG:
		type = "GnuPG";
		break;
	case PGP_TYPE_PGP2:
		type = "PGP 2.6.x";
		break;
	case PGP_TYPE_PGP5:
		type = "PGP 5.0";
		break;
	case PGP_TYPE_PGP6:
		type = "PGP 6.5.8";
		break;
	}
	
	prompt = g_strdup_printf (_("Please enter your %s passphrase%s%s"),
				  type, userid ? _(" for ") : "",
				  userid ? userid : "");
	
	passphrase = pgp_passwd_func (prompt, pgp_data);
	g_free (prompt);
	
	return passphrase;
}


struct {
	char *bin;
	char *version;
	PgpType type;
} binaries[] = {
	{ "gpg", NULL, PGP_TYPE_GPG },
	{ "pgp", "6.5.8", PGP_TYPE_PGP6 },
	{ "pgp", "5.0", PGP_TYPE_PGP5 },
	{ "pgp", "2.6", PGP_TYPE_PGP2 },
	{ NULL, NULL, PGP_TYPE_NONE }
};


typedef struct _PGPFILE {
	FILE *fp;
	pid_t pid;
} PGPFILE;

static PGPFILE *
pgpopen (const char *command, const char *mode)
{
	int in_fds[2], out_fds[2];
	PGPFILE *pgp = NULL;
	char **argv = NULL;
	pid_t child;
	int fd;
	
	g_return_val_if_fail (command != NULL, NULL);
	
	if (*mode != 'r' || *mode != 'w')
		return NULL;
	
	argv = g_strsplit (command, " ", 0);
	if (!argv)
		return NULL;
	
	if (pipe (in_fds) == -1)
		goto error;
	
	if (pipe (out_fds) == -1) {
		close (in_fds[0]);
		close (in_fds[1]);
		goto error;
	}
	
	if ((child = fork ()) == 0) {
		/* In child */
		int maxfd;
		
		if ((dup2 (in_fds[0], STDIN_FILENO) < 0 ) ||
		    (dup2 (out_fds[1], STDOUT_FILENO) < 0 ) ||
		    (dup2 (out_fds[1], STDERR_FILENO) < 0 )) {
			_exit (255);
		}
		
		/* Dissociate from evolution-mail's controlling
		 * terminal so that pgp/gpg won't be able to read from
		 * it: PGP 2 will fall back to asking for the password
		 * on /dev/tty if the passed-in password is incorrect.
		 * This will make that fail rather than hanging.
		 */
		setsid ();
		
		/* close all open fds that we aren't using */
		maxfd = sysconf (_SC_OPEN_MAX);
		for (fd = 0; fd < maxfd; fd++) {
			if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
				close (fd);
		}
		
		execvp (argv[0], argv);
		fprintf (stderr, "Could not execute %s: %s\n", argv[0],
			 g_strerror (errno));
		_exit (255);
	} else if (child < 0) {
		close (in_fds[0]);
		close (in_fds[1]);
		close (out_fds[0]);
		close (out_fds[1]);
		goto error;
	}
	
	/* Parent */
	g_strfreev (argv);
	
	close (in_fds[0]);   /* pgp's stdin */
	close (out_fds[1]);  /* pgp's stdout */
	
	if (mode[0] == 'r') {
		/* opening in read-mode */
		fd = out_fds[0];
		close (in_fds[1]);
	} else {
		/* opening in write-mode */
		fd = in_fds[1];
		close (out_fds[0]);
	}
	
	pgp = g_new (PGPFILE, 1);
	pgp->fp = fdopen (fd, mode);
	pgp->pid = child;
	
	return pgp;
 error:
	g_strfreev (argv);
	
	return NULL;
}

static int
pgpclose (PGPFILE *pgp)
{
	sigset_t mask, omask;
	pid_t wait_result;
	int status;
	
	if (pgp->fp) {
		fclose (pgp->fp);
		pgp->fp = NULL;
	}
	
	/* PGP5 closes fds before exiting, meaning this might be called
	 * too early. So wait a bit for the result.
	 */
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigprocmask (SIG_BLOCK, &mask, &omask);
	alarm (1);
	wait_result = waitpid (pgp->pid, &status, 0);
	alarm (0);
	sigprocmask (SIG_SETMASK, &omask, NULL);
	
	if (wait_result == -1 && errno == EINTR) {
		/* PGP is hanging: send a friendly reminder. */
		kill (pgp->pid, SIGTERM);
		sleep (1);
		wait_result = waitpid (pgp->pid, &status, WNOHANG);
		if (wait_result == 0) {
			/* Still hanging; use brute force. */
			kill (pgp->pid, SIGKILL);
			sleep (1);
			wait_result = waitpid (pgp->pid, &status, WNOHANG);
		}
	}
	
	if (wait_result != -1 && WIFEXITED (status)) {
		g_free (pgp);
		return 0;
	} else
		return -1;
}

PgpType
pgp_type_detect_from_path (const char *path)
{
	const char *bin = g_basename (path);
	struct stat st;
	int i;
	
	/* make sure the file exists *and* is executable? */
	if (stat (path, &st) == -1 || !(st.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
		return PGP_TYPE_NONE;
	
	for (i = 0; binaries[i].bin; i++) {
		if (binaries[i].version) {
			/* compare version strings */
			char buffer[256], *command;
			gboolean found = FALSE;
			PGPFILE *fp;
			
			command = g_strdup_printf ("%s --version", path);
			fp = pgpopen (command, "r");
			g_free (command);
			if (fp) {
				while (!feof (fp->fp) && !found) {
					memset (buffer, 0, sizeof (buffer));
					fgets (buffer, sizeof (buffer), fp->fp);
					found = strstr (buffer, binaries[i].version) != NULL;
				}
				
				pgpclose (fp);
				
				if (found)
					return binaries[i].type;
			}
		} else if (!strcmp (binaries[i].bin, bin)) {
			/* no version string to compare against... */
			return binaries[i].type;
		}
	}
	
	return PGP_TYPE_NONE;
}

void
pgp_autodetect (char **pgp_path, PgpType *pgp_type)
{
	PgpType type = PGP_TYPE_NONE;
	const char *PATH, *path;
	char *pgp = NULL;
	
	PATH = getenv ("PATH");
	
	path = PATH;
	while (path && *path && !type) {
		const char *pend = strchr (path, ':');
		gboolean found = FALSE;
		char *dirname;
		int i;
		
		if (pend) {
			/* don't even think of using "." */
			if (!strncmp (path, ".", pend - path)) {
				path = pend + 1;
				continue;
			}
			
			dirname = g_strndup (path, pend - path);
			path = pend + 1;
		} else {
			/* don't even think of using "." */
			if (!strcmp (path, "."))
				break;
			
			dirname = g_strdup (path);
			path = NULL;
		}
		
		for (i = 0; binaries[i].bin; i++) {
			struct stat st;
			
			pgp = g_strdup_printf ("%s/%s", dirname, binaries[i].bin);
			/* make sure the file exists *and* is executable? */
			if (stat (pgp, &st) != -1 && st.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)) {
				if (binaries[i].version) {
					/* compare version strings */
					char buffer[256], *command;
					PGPFILE *fp;
					
					command = g_strdup_printf ("%s --version", pgp);
					fp = pgpopen (command, "r");
					g_free (command);
					if (fp) {
						while (!feof (fp->fp) && !found) {
							memset (buffer, 0, sizeof (buffer));
							fgets (buffer, sizeof (buffer), fp->fp);
							found = strstr (buffer, binaries[i].version) != NULL;
						}
						
						pgpclose (fp);
					}
				} else {
					/* no version string to compare against... */
					found = TRUE;
				}
				
				if (found) {
					type = binaries[i].type;
					break;
				}
			}
			
			g_free (pgp);
			pgp = NULL;
		}
		
		g_free (dirname);
	}
	
	if (pgp && type) {
		*pgp_path = pgp;
		*pgp_type = type;
	} else
		g_free (pgp);
}


/**
 * pgp_init:
 * @path: path to pgp
 * @type: pgp program type
 * @callback: function to query for a passphrase
 * @data: user data
 *
 * Initializes pgp variables
 **/
void
pgp_init (const char *path, PgpType type, PgpPasswdFunc callback, gpointer data)
{
	pgp_path = g_strdup (path);
	pgp_type = type;
	pgp_passwd_func = callback;
	pgp_data = data;
}


/**
 * pgp_detect:
 * @text: input text
 *
 * Returns TRUE if it is found that the text contains a PGP encrypted
 * block otherwise returns FALSE.
 **/
gboolean
pgp_detect (const char *text)
{
	if (strstr (text, "-----BEGIN PGP MESSAGE-----"))
		return TRUE;
	return FALSE;
}


/**
 * pgp_sign_detect:
 * @text: input text
 *
 * Returns TRUE if it is found that the text contains a PGP signed
 * block otherwise returns FALSE.
 **/
gboolean
pgp_sign_detect (const char *text)
{
	if (strstr (text, "-----BEGIN PGP SIGNED MESSAGE-----"))
		return TRUE;
	return FALSE;
}

static int
cleanup_child (pid_t child)
{
	int status;
	pid_t wait_result;
	sigset_t mask, omask;
	
	/* PGP5 closes fds before exiting, meaning this might be called
	 * too early. So wait a bit for the result.
	 */
	sigemptyset (&mask);
	sigaddset (&mask, SIGALRM);
	sigprocmask (SIG_BLOCK, &mask, &omask);
	alarm (1);
	wait_result = waitpid (child, &status, 0);
	alarm (0);
	sigprocmask (SIG_SETMASK, &omask, NULL);
	
	if (wait_result == -1 && errno == EINTR) {
		/* The child is hanging: send a friendly reminder. */
		kill (child, SIGTERM);
		sleep (1);
		wait_result = waitpid (child, &status, WNOHANG);
		if (wait_result == 0) {
			/* Still hanging; use brute force. */
			kill (child, SIGKILL);
			sleep (1);
			wait_result = waitpid (child, &status, WNOHANG);
		}
	}
	
	if (wait_result != -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}

static void
cleanup_before_exec (int fd)
{
	int maxfd, i;
	
	maxfd = sysconf (_SC_OPEN_MAX);
	if (maxfd < 0)
		return;
	
	/* Loop over all fds. */
	for (i = 0; i < maxfd; i++) {
		if ((STDIN_FILENO != i) &&
		    (STDOUT_FILENO != i) &&
		    (STDERR_FILENO != i) &&
		    (fd != i))
			close (i);
	}
}

static int
crypto_exec_with_passwd (const char *path, char *argv[], const char *input, int inlen,
			 int passwd_fds[], const char *passphrase,
			 char **output, int *outlen, char **diagnostics)
{
	fd_set fdset, write_fdset;
	int ip_fds[2], op_fds[2], diag_fds[2];
	int select_result, read_len, write_len;
	size_t tmp_len;
	pid_t child;
	char *buf, *diag_buf;
	const char *passwd_next, *input_next;
	size_t size, alloc_size, diag_size, diag_alloc_size;
	gboolean eof_seen, diag_eof_seen, passwd_eof_seen, input_eof_seen;
	size_t passwd_remaining, passwd_incr, input_remaining, input_incr;
	struct timeval timeout;
	
	
	if ((pipe (ip_fds) < 0 ) ||
	    (pipe (op_fds) < 0 ) ||
	    (pipe (diag_fds) < 0 )) {
		*diagnostics = g_strdup_printf ("Couldn't create pipe to %s: "
						"%s", pgp_path,
						g_strerror (errno));
		return 0;
	}
	
	if (!(child = fork ())) {
		/* In child */
		
		if ((dup2 (ip_fds[0], STDIN_FILENO) < 0 ) ||
		    (dup2 (op_fds[1], STDOUT_FILENO) < 0 ) ||
		    (dup2 (diag_fds[1], STDERR_FILENO) < 0 )) {
			_exit (255);
		}
		
		/* Dissociate from evolution-mail's controlling
		 * terminal so that pgp/gpg won't be able to read from
		 * it: PGP 2 will fall back to asking for the password
		 * on /dev/tty if the passed-in password is incorrect.
		 * This will make that fail rather than hanging.
		 */
		setsid ();
		
		/* Close excess fds */
		cleanup_before_exec (passwd_fds[0]);
		
		execvp (path, argv);
		fprintf (stderr, "Could not execute %s: %s\n", argv[0],
			 g_strerror (errno));
		_exit (255);
	} else if (child < 0) {
		*diagnostics = g_strdup_printf ("Cannot fork %s: %s",
						argv[0], g_strerror (errno));
		return 0;
	}
	
	/* Parent */
	close (ip_fds[0]);
	close (op_fds[1]);
	close (diag_fds[1]);
	close (passwd_fds[0]);
	
	timeout.tv_sec = 10; /* timeout in seconds */
	timeout.tv_usec = 0;
	
	size = diag_size = 0;
	alloc_size = 4096;
	diag_alloc_size = 1024;
	eof_seen = diag_eof_seen = FALSE;
	
	buf = g_malloc (alloc_size);
	diag_buf = g_malloc (diag_alloc_size);
	
	passwd_next = passphrase;
	passwd_remaining = passphrase ? strlen (passphrase) : 0;
	passwd_incr = fpathconf (passwd_fds[1], _PC_PIPE_BUF);
	/* Use a reasonable default value on error. */
	if (passwd_incr <= 0)
		passwd_incr = 1024;
	passwd_eof_seen = FALSE;
	
	input_next = input;
	input_remaining = inlen;
	input_incr = fpathconf (ip_fds[1], _PC_PIPE_BUF);
	if (input_incr <= 0)
		input_incr = 1024;
	input_eof_seen = FALSE;
	
	while (!(eof_seen && diag_eof_seen)) {
		FD_ZERO (&fdset);
		if (!eof_seen)
			FD_SET (op_fds[0], &fdset);
		if (!diag_eof_seen)
			FD_SET (diag_fds[0], &fdset);
		
		FD_ZERO (&write_fdset);
		if (!passwd_eof_seen)
			FD_SET (passwd_fds[1], &write_fdset);
		if (!input_eof_seen)
			FD_SET (ip_fds[1], &write_fdset);
		
		select_result = select (FD_SETSIZE, &fdset, &write_fdset,
					NULL, &timeout);
		if (select_result < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (select_result == 0) {
			/* timeout */
			break;
		}
		
		if (FD_ISSET (op_fds[0], &fdset)) {
			/* More output is available. */
			
			if (size + 4096 > alloc_size) {
				alloc_size += 4096;
				buf = g_realloc (buf , alloc_size);
			}
			read_len = read (op_fds[0], &buf[size],
					 alloc_size - size - 1);
			if (read_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (read_len == 0)
				eof_seen = TRUE;
			size += read_len;
		}
		
		if (FD_ISSET(diag_fds[0], &fdset) ) {
			/* More stderr is available. */
			
			if (diag_size + 1024 > diag_alloc_size) {
				diag_alloc_size += 1024;
				diag_buf = g_realloc (diag_buf,
						      diag_alloc_size);
			}
			
			read_len = read (diag_fds[0], &diag_buf[diag_size],
					 diag_alloc_size - diag_size - 1);
			if (read_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (read_len == 0)
				diag_eof_seen = TRUE;
			diag_size += read_len;
		}
		
		if (FD_ISSET(passwd_fds[1], &write_fdset)) {
			/* Ready for more password input. */
			
			tmp_len = passwd_incr;
			if (tmp_len > passwd_remaining)
				tmp_len = passwd_remaining;
			write_len = write (passwd_fds[1], passwd_next,
					   tmp_len);
			if (write_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			passwd_next += write_len;
			passwd_remaining -= write_len;
			if (passwd_remaining == 0) {
				close (passwd_fds[1]);
				passwd_eof_seen = TRUE;
			}
		}
		
		if (FD_ISSET(ip_fds[1], &write_fdset)) {
			/* Ready for more ciphertext input. */
			
			tmp_len = input_incr;
			if (tmp_len > input_remaining)
				tmp_len = input_remaining;
			write_len = write (ip_fds[1], input_next, tmp_len);
			if (write_len < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			input_next += write_len;
			input_remaining -= write_len;
			if (input_remaining == 0 ) {
				close (ip_fds[1]);
				input_eof_seen = TRUE;
			}
		}
	}
	
	buf[size] = 0;
	diag_buf[diag_size] = 0;
	close (op_fds[0]);
	close (diag_fds[0]);
	
	*output = buf;
	if (outlen)
		*outlen = size;
	*diagnostics = diag_buf;
	
	return cleanup_child (child);
}

/*----------------------------------------------------------------------*
 *                     Public crypto functions
 *----------------------------------------------------------------------*/

/**
 * pgp_decrypt:
 * @ciphertext: ciphertext to decrypt
 * @cipherlen: ciphertext length
 * @outlen: output length of the decrypted data (to be set by #pgp_decrypt)
 * @ex: exception
 *
 * Returns an allocated buffer containing the decrypted ciphertext. If
 * the cleartext is plain text then you may treat it like a normal
 * string as it will be NUL terminated, however #outlen is also set in
 * the case that the cleartext is a binary stream.
 **/
char *
pgp_decrypt (const char *ciphertext, size_t cipherlen, size_t *outlen, GMimeException *ex)
{
	char *argv[15];
	char *plaintext = NULL;
	char *diagnostics = NULL;
	char *passphrase;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	if (pgp_type == PGP_TYPE_NONE) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("No GPG/PGP program available."));
		return NULL;
	}
	
	passphrase = pgp_get_passphrase (NULL);
	if (!passphrase) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("No password provided."));
		return NULL;
	}
	
	if (pipe (passwd_fds) < 0) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("Couldn't create pipe to GPG/PGP: %s"),
				       g_strerror (errno));
		return NULL;
	}
	
	i = 0;
	switch (pgp_type) {
	case PGP_TYPE_GPG:
		argv[i++] = "gpg";
		argv[i++] = "--verbose";
		argv[i++] = "--yes";
		argv[i++] = "--batch";
		
		argv[i++] = "--output";
		argv[i++] = "-";            /* output to stdout */
		
		argv[i++] = "--decrypt";
		
		argv[i++] = "--passphrase-fd";
		sprintf (passwd_fd, "%d", passwd_fds[0]);
		argv[i++] = passwd_fd;
		break;
	case PGP_TYPE_PGP5:
		argv[i++] = "pgpv";
		argv[i++] = "-f";
		argv[i++] = "+batchmode=1";
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	case PGP_TYPE_PGP2:
		argv[i++] = "pgp";
		argv[i++] = "-f";
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	}
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (pgp_path, argv,
					  ciphertext, cipherlen,
					  passwd_fds, passphrase,
					  &plaintext, outlen,
					  &diagnostics);
	g_free (passphrase);
	
	if (retval != 0 || !*plaintext) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       "%s", diagnostics);
		g_free (plaintext);
		g_free (diagnostics);
		return NULL;
	}
	
	g_free (diagnostics);
	
	return plaintext;
}


/**
 * pgp_encrypt:
 * @in: data to encrypt
 * @inlen: input length of input data
 * @recipients: An array of recipient ids
 * @sign: TRUE if you want to sign as well as encrypt
 * @userid: userid to use when signing (assuming #sign is TRUE)
 * @ex: exception
 *
 * Returns an allocated string containing the ciphertext.
 **/
char *
pgp_encrypt (const char *in, size_t inlen, const GPtrArray *recipients,
	     gboolean sign, const char *userid, GMimeException *ex)
{
	GPtrArray *recipient_list = NULL;
	GPtrArray *argv = NULL;
	int retval, r;
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	char *passphrase = NULL;
	
	if (pgp_type == PGP_TYPE_NONE) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("No GPG/PGP program available."));
		return NULL;
	}
	
	if (sign) {
		/* we only need a passphrase if we intend on signing */
		passphrase = pgp_get_passphrase (NULL);
		if (!passphrase) {
			g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
					      _("No password provided."));
			return NULL;
		}
	}
	
	if (pipe (passwd_fds) < 0) {
		g_free (passphrase);
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("Couldn't create pipe to GPG/PGP: %s"),
				       g_strerror (errno));
		return NULL;
	}
	
	argv = g_ptr_array_new ();
	switch (pgp_type) {
	case PGP_TYPE_GPG:
		if (recipients->len == 0) {
			g_free (passphrase);
			g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
					       _("No recipients specified"));
			return NULL;
		}
		
		recipient_list = g_ptr_array_new ();
		for (r = 0; r < recipients->len; r++) {
			char *buf, *recipient;
			
			recipient = recipients->pdata[r];
			buf = g_strdup_printf ("-r %s", recipient);
			g_ptr_array_add (recipient_list, buf);
			g_free (recipient);
		}
		
		g_ptr_array_add (argv, "gpg");
		
		g_ptr_array_add (argv, "--verbose");
		g_ptr_array_add (argv, "--yes");
		g_ptr_array_add (argv, "--batch");
		
		g_ptr_array_add (argv, "--armor");
		
		for (r = 0; r < recipient_list->len; r++)
			g_ptr_array_add (argv, recipient_list->pdata[r]);
		
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");            /* output to stdout */
		
		g_ptr_array_add (argv, "--encrypt");
		
		if (sign) {
			g_ptr_array_add (argv, "--sign");
			
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (gchar *) userid);
			
			g_ptr_array_add (argv, "--passphrase-fd");
			sprintf (passwd_fd, "%d", passwd_fds[0]);
			g_ptr_array_add (argv, passwd_fd);
		}
		break;
	case PGP_TYPE_PGP5:
		if (recipients->len == 0) {
			g_free (passphrase);
			g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
					       _("No recipients specified"));
			return NULL;
		}
		
		recipient_list = g_ptr_array_new ();
		for (r = 0; r < recipients->len; r++) {
			char *buf, *recipient;
			
			recipient = recipients->pdata[r];
			buf = g_strdup_printf ("-r %s", recipient);
			g_ptr_array_add (recipient_list, buf);
			g_free (recipient);
		}
		
		g_ptr_array_add (argv, "pgpe");
		
		for (r = 0; r < recipient_list->len; r++)
			g_ptr_array_add (argv, recipient_list->pdata[r]);
		
		g_ptr_array_add (argv, "-f");
		g_ptr_array_add (argv, "-z");
		g_ptr_array_add (argv, "-a");
		g_ptr_array_add (argv, "-o");
		g_ptr_array_add (argv, "-");        /* output to stdout */
		
		if (sign) {
			g_ptr_array_add (argv, "-s");
			
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (gchar *) userid);
			
			sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
			putenv (passwd_fd);
		}
		break;
	case PGP_TYPE_PGP2:
		if (recipients->len == 0) {
			g_free (passphrase);
			g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
					       _("No recipients specified"));
			return NULL;
		}
		
		recipient_list = g_ptr_array_new ();
		for (r = 0; r < recipients->len; r++) {
			char *buf, *recipient;
			
			recipient = recipients->pdata[r];
			buf = g_strdup (recipient);
			g_ptr_array_add (recipient_list, buf);
			g_free (recipient);
		}
		
		g_ptr_array_add (argv, "pgp");
		g_ptr_array_add (argv, "-f");
		g_ptr_array_add (argv, "-e");
		g_ptr_array_add (argv, "-a");
		g_ptr_array_add (argv, "-o");
		g_ptr_array_add (argv, "-");
		
		for (r = 0; r < recipient_list->len; r++)
			g_ptr_array_add (argv, recipient_list->pdata[r]);
		
		if (sign) {
			g_ptr_array_add (argv, "-s");
			
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (gchar *) userid);
			
			sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
			putenv (passwd_fd);
		}
		break;
	}
	
	g_ptr_array_add (argv, NULL);
	
	retval = crypto_exec_with_passwd (pgp_path, (char **) argv->pdata,
					  in, inlen, passwd_fds,
					  passphrase, &ciphertext, NULL,
					  &diagnostics);
	
	g_free (passphrase);
	g_ptr_array_free (argv, TRUE);
	
	if (retval != 0 || !*ciphertext) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       "%s", diagnostics);
		g_free (ciphertext);
		ciphertext = NULL;
	}
	
	if (recipient_list) {
		for (r = 0; r < recipient_list->len; r++)
			g_free (recipient_list->pdata[r]);
		g_ptr_array_free (recipient_list, TRUE);
	}
	
	g_free (diagnostics);
	
	return ciphertext;
}


/**
 * pgp_clearsign:
 * @plaintext: plain readable text to clearsign
 * @userid: userid to sign with
 * @hash: Preferred hash function (md5 or sha1)
 * @ex: exception
 *
 * Returns an allocated string containing the clearsigned plaintext
 * using the preferred hash.
 **/
char *
pgp_clearsign (const char *plaintext, const char *userid,
	       PgpHashType hash, GMimeException *ex)
{
	char *argv[15];
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	char *passphrase = NULL;
	char *hash_str = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	if (pgp_type == PGP_TYPE_NONE) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("No GPG/PGP program available."));
		return NULL;
	}
	
	passphrase = pgp_get_passphrase (userid);
	if (!passphrase) {
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      _("No password provided."));
		return NULL;
	}
	
	if (pipe (passwd_fds) < 0) {
		g_free (passphrase);
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("Couldn't create pipe to GPG/PGP: %s"),
				       g_strerror (errno));
		return NULL;
	}
	
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_str = "MD5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_str = "SHA1";
		break;
	}
	
	i = 0;
	switch (pgp_type) {
	case PGP_TYPE_GPG:
		argv[i++] = "gpg";
		
		argv[i++] = "--clearsign";
		
		if (hash_str) {
			argv[i++] = "--digest-algo";
			argv[i++] = hash_str;
		}
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "--verbose";
		argv[i++] = "--yes";
		argv[i++] = "--batch";
		
		argv[i++] = "--armor";
		
		argv[i++] = "--output";
		argv[i++] = "-";            /* output to stdout */
		
		argv[i++] = "--passphrase-fd";
		sprintf (passwd_fd, "%d", passwd_fds[0]);
		argv[i++] = passwd_fd;
		break;
	case PGP_TYPE_PGP5:
		/* FIXME: modify to respect hash */
		argv[i++] = "pgps";
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "-f";
		argv[i++] = "-z";
		argv[i++] = "-a";
		argv[i++] = "-o";
		argv[i++] = "-";        /* output to stdout */
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	case PGP_TYPE_PGP2:
		/* FIXME: modify to respect hash */
		argv[i++] = "pgp";
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "-f";
		argv[i++] = "-a";
		argv[i++] = "-o";
		argv[i++] = "-";
		
		argv[i++] = "-st";
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	}
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (pgp_path, argv,
					  plaintext, strlen (plaintext),
					  passwd_fds, passphrase,
					  &ciphertext, NULL,
					  &diagnostics);
	
	g_free (passphrase);
	
	if (retval != 0 || !*ciphertext) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       "%s", diagnostics);
		g_free (ciphertext);
		ciphertext = NULL;
	}
	
	g_free (diagnostics);
	
	return ciphertext;
}


/**
 * pgp_sign:
 * @in: input data to sign
 * @inlen: length of input data
 * @userid: userid to sign with
 * @hash: preferred hash type (md5 or sha1)
 * @ex: exception
 *
 * Returns an allocated string containing the detached signature using
 * the preferred hash.
 **/
char *
pgp_sign (const char *in, size_t inlen, const char *userid,
	  PgpHashType hash, GMimeException *ex)
{
	char *argv[20];
	char *ciphertext = NULL;
	char *diagnostics = NULL;
	char *passphrase = NULL;
	char *hash_str = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	int retval, i;
	
	if (pgp_type == PGP_TYPE_NONE) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("No GPG/PGP program available."));
		return NULL;
	}
	
	passphrase = pgp_get_passphrase (userid);
	if (!passphrase) {
		g_mime_exception_set (ex, GMIME_EXCEPTION_SYSTEM,
				      _("No password provided."));
		return NULL;
	}
	
	if (pipe (passwd_fds) < 0) {
		g_free (passphrase);
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("Couldn't create pipe to GPG/PGP: %s"),
				       g_strerror (errno));
		return NULL;
	}
	
	switch (hash) {
	case PGP_HASH_TYPE_MD5:
		hash_str = "MD5";
		break;
	case PGP_HASH_TYPE_SHA1:
		hash_str = "SHA1";
		break;
	}
	
	i = 0;
	switch (pgp_type) {
	case PGP_TYPE_GPG:
		argv[i++] = "gpg";
		
		argv[i++] = "--sign";
		argv[i++] = "-b";
		if (hash_str) {
			argv[i++] = "--digest-algo";
			argv[i++] = hash_str;
		}
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "--verbose";
		argv[i++] = "--yes";
		argv[i++] = "--batch";
		
		argv[i++] = "--armor";
		
		argv[i++] = "--output";
		argv[i++] = "-";            /* output to stdout */
		
		argv[i++] = "--passphrase-fd";
		sprintf (passwd_fd, "%d", passwd_fds[0]);
		argv[i++] = passwd_fd;
		break;
	case PGP_TYPE_PGP5:
		/* FIXME: respect hash */
		argv[i++] = "pgps";
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "-b";
		argv[i++] = "-f";
		argv[i++] = "-z";
		argv[i++] = "-a";
		argv[i++] = "-o";
		argv[i++] = "-";        /* output to stdout */
		
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	case PGP_TYPE_PGP2:
		/* FIXME: respect hash */
		argv[i++] = "pgp";
		
		if (userid) {
			argv[i++] = "-u";
			argv[i++] = (char *) userid;
		}
		
		argv[i++] = "-f";
		argv[i++] = "-a";
		argv[i++] = "-o";
		argv[i++] = "-";
		
		argv[i++] = "-sb"; /* create a detached signature */
		sprintf (passwd_fd, "PGPPASSFD=%d", passwd_fds[0]);
		putenv (passwd_fd);
		break;
	}
	
	argv[i++] = NULL;
	
	retval = crypto_exec_with_passwd (pgp_path, argv,
					  in, inlen,
					  passwd_fds, passphrase,
					  &ciphertext, NULL,
					  &diagnostics);
	
	g_free (passphrase);
	
	if (retval != 0 || !*ciphertext) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       "%s", diagnostics);
		g_free (ciphertext);
		ciphertext = NULL;
	}
	
	g_free (diagnostics);
	
	return ciphertext;
}

static char *
swrite (const char *data, int len)
{
	char *template;
	int fd;
	
	template = g_strdup ("/tmp/gmime-crypto-XXXXXX");
	fd = mkstemp (template);
	if (fd == -1) {
		g_free (template);
		return NULL;
	}
	
	write (fd, data, len);
	close (fd);
	
	return template;
}

gboolean
pgp_verify (const char *in, size_t inlen, const char *sigin, size_t siglen, GMimeException *ex)
{
	char *argv[20];
	char *cleartext = NULL;
	char *diagnostics = NULL;
	int passwd_fds[2];
	char passwd_fd[32];
	char *sigfile = NULL;
	int retval, i, clearlen;
	gboolean valid = TRUE;
	
	
	if (pgp_type == PGP_TYPE_NONE) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("No GPG/PGP program available."));
		return FALSE;
	}
	
	if (pipe (passwd_fds) < 0) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       _("Couldn't create pipe to GPG/PGP: %s"),
				       g_strerror (errno));
		return FALSE;
	}
	
	if (sigin != NULL && siglen) {
		/* We are going to verify a detached signature so save
		   the signature to a temp file. */
		sigfile = swrite (sigin, siglen);
		if (!sigfile) {
			g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
					       _("Couldn't create temp file: %s"),
					       g_strerror (errno));
			return FALSE;
		}
	}
	
	i = 0;
	switch (pgp_type) {
	case PGP_TYPE_GPG:
		argv[i++] = "gpg";
		
		argv[i++] = "--verify";
		
		if (sigin != NULL && siglen)
			argv[i++] = sigfile;
		
		argv[i++] = "-";
		
		/*argv[i++] = "--verbose";*/
		/*argv[i++] = "--yes";*/
		/*argv[i++] = "--batch";*/
		break;
	case PGP_TYPE_PGP5:
		argv[i++] = "pgpv";
		
		argv[i++] = "-z";
		
		if (sigin != NULL && siglen)
			argv[i++] = sigfile;
		
		argv[i++] = "-f";
		
		break;
	case PGP_TYPE_PGP2:
		argv[i++] = "pgp";
		
		if (sigin != NULL && siglen)
			argv[i++] = sigfile;
		
		argv[i++] = "-f";
		
		break;
	}
	
	argv[i++] = NULL;
	
	clearlen = 0;
	retval = crypto_exec_with_passwd (pgp_path, argv,
					  in, inlen,
					  passwd_fds, NULL,
					  &cleartext, &clearlen,
					  &diagnostics);
	
	/* cleanup */
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	/* FIXME: maybe we should always set an exception? */
	if (retval != 0) {
		g_mime_exception_setv (ex, GMIME_EXCEPTION_SYSTEM,
				       "%s", diagnostics);
		valid = FALSE;
	}
	
	g_free (diagnostics);
	g_free (cleartext);
	
	return valid;
}
