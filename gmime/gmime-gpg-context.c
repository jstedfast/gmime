/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2009 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include "gmime-gpg-context.h"
#include "gmime-filter-charset.h"
#include "gmime-stream-filter.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-fs.h"
#include "gmime-charset.h"
#include "gmime-error.h"

#ifdef ENABLE_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define _(x) x


/**
 * SECTION: gmime-gpg-context
 * @title: GMimeGpgContext
 * @short_description: GnuPG cipher contexts
 * @see_also: #GMimeCipherContext
 *
 * A #GMimeGpgContext is a #GMimeCipherContext that uses GnuPG to do
 * all of the encryption and digital signatures.
 **/


static void g_mime_gpg_context_class_init (GMimeGpgContextClass *klass);
static void g_mime_gpg_context_init (GMimeGpgContext *ctx, GMimeGpgContextClass *klass);
static void g_mime_gpg_context_finalize (GObject *object);

static GMimeCipherHash gpg_hash_id (GMimeCipherContext *ctx, const char *hash);

static const char *gpg_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash);

static int gpg_sign (GMimeCipherContext *ctx, const char *userid,
		     GMimeCipherHash hash, GMimeStream *istream,
		     GMimeStream *ostream, GError **err);
	
static GMimeSignatureValidity *gpg_verify (GMimeCipherContext *ctx, GMimeCipherHash hash,
					   GMimeStream *istream, GMimeStream *sigstream,
					   GError **err);

static int gpg_encrypt (GMimeCipherContext *ctx, gboolean sign,
			const char *userid, GPtrArray *recipients,
			GMimeStream *istream, GMimeStream *ostream,
			GError **err);

static GMimeSignatureValidity *gpg_decrypt (GMimeCipherContext *ctx, GMimeStream *istream,
					    GMimeStream *ostream, GError **err);

static int gpg_import_keys (GMimeCipherContext *ctx, GMimeStream *istream,
			    GError **err);

static int gpg_export_keys (GMimeCipherContext *ctx, GPtrArray *keys,
			    GMimeStream *ostream, GError **err);


static GMimeCipherContextClass *parent_class = NULL;


GType
g_mime_gpg_context_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeGpgContextClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_gpg_context_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeGpgContext),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_gpg_context_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_CIPHER_CONTEXT, "GMimeGpgContext", &info, 0);
	}
	
	return type;
}


static void
g_mime_gpg_context_class_init (GMimeGpgContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeCipherContextClass *cipher_class = GMIME_CIPHER_CONTEXT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_gpg_context_finalize;
	
	cipher_class->hash_id = gpg_hash_id;
	cipher_class->hash_name = gpg_hash_name;
	cipher_class->sign = gpg_sign;
	cipher_class->verify = gpg_verify;
	cipher_class->encrypt = gpg_encrypt;
	cipher_class->decrypt = gpg_decrypt;
	cipher_class->import_keys = gpg_import_keys;
	cipher_class->export_keys = gpg_export_keys;
}

static void
g_mime_gpg_context_init (GMimeGpgContext *ctx, GMimeGpgContextClass *klass)
{
	GMimeCipherContext *cipher = (GMimeCipherContext *) ctx;
	
	ctx->path = NULL;
	ctx->always_trust = FALSE;
	
	cipher->sign_protocol = "application/pgp-signature";
	cipher->encrypt_protocol = "application/pgp-encrypted";
	cipher->key_protocol = "application/pgp-keys";
}

static void
g_mime_gpg_context_finalize (GObject *object)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) object;
	
	g_free (ctx->path);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GMimeCipherHash
gpg_hash_id (GMimeCipherContext *ctx, const char *hash)
{
	if (hash == NULL)
		return GMIME_CIPHER_HASH_DEFAULT;
	
	if (!g_ascii_strcasecmp (hash, "pgp-"))
		hash += 4;
	
	if (!g_ascii_strcasecmp (hash, "md2"))
		return GMIME_CIPHER_HASH_MD2;
	else if (!g_ascii_strcasecmp (hash, "md5"))
		return GMIME_CIPHER_HASH_MD5;
	else if (!g_ascii_strcasecmp (hash, "sha1"))
		return GMIME_CIPHER_HASH_SHA1;
	else if (!g_ascii_strcasecmp (hash, "sha224"))
		return GMIME_CIPHER_HASH_SHA224;
	else if (!g_ascii_strcasecmp (hash, "sha256"))
		return GMIME_CIPHER_HASH_SHA256;
	else if (!g_ascii_strcasecmp (hash, "sha384"))
		return GMIME_CIPHER_HASH_SHA384;
	else if (!g_ascii_strcasecmp (hash, "sha512"))
		return GMIME_CIPHER_HASH_SHA512;
	else if (!g_ascii_strcasecmp (hash, "ripemd160"))
		return GMIME_CIPHER_HASH_RIPEMD160;
	else if (!g_ascii_strcasecmp (hash, "tiger192"))
		return GMIME_CIPHER_HASH_TIGER192;
	else if (!g_ascii_strcasecmp (hash, "haval-5-160"))
		return GMIME_CIPHER_HASH_HAVAL5160;
	
	return GMIME_CIPHER_HASH_DEFAULT;
}

static const char *
gpg_hash_name (GMimeCipherContext *ctx, GMimeCipherHash hash)
{
	switch (hash) {
	case GMIME_CIPHER_HASH_MD2:
		return "pgp-md2";
	case GMIME_CIPHER_HASH_MD5:
		return "pgp-md5";
	case GMIME_CIPHER_HASH_SHA1:
		return "pgp-sha1";
	case GMIME_CIPHER_HASH_SHA224:
		return "pgp-sha224";
	case GMIME_CIPHER_HASH_SHA256:
		return "pgp-sha256";
	case GMIME_CIPHER_HASH_SHA384:
		return "pgp-sha384";
	case GMIME_CIPHER_HASH_SHA512:
		return "pgp-sha512";
	case GMIME_CIPHER_HASH_RIPEMD160:
		return "pgp-ripemd160";
	case GMIME_CIPHER_HASH_TIGER192:
		return "pgp-tiger192";
	case GMIME_CIPHER_HASH_HAVAL5160:
		return "pgp-haval-5-160";
	default:
		return "pgp-sha1";
	}
}


enum _GpgCtxMode {
	GPG_CTX_MODE_SIGN,
	GPG_CTX_MODE_VERIFY,
	GPG_CTX_MODE_ENCRYPT,
	GPG_CTX_MODE_SIGN_ENCRYPT,
	GPG_CTX_MODE_DECRYPT,
	GPG_CTX_MODE_IMPORT,
	GPG_CTX_MODE_EXPORT,
};

struct _GpgCtx {
	enum _GpgCtxMode mode;
	GMimeSession *session;
	GHashTable *userid_hint;
	pid_t pid;
	
	char *path;
	char *userid;
	char *sigfile;
	GPtrArray *recipients;
	GMimeCipherHash hash;
	
	int stdin_fd;
	int stdout_fd;
	int stderr_fd;
	int status_fd;
	int passwd_fd;  /* only needed for sign/decrypt */
	
	/* status-fd buffer */
	char *statusbuf;
	char *statusptr;
	guint statusleft;
	
	char *need_id;
	char *passwd;
	
	GMimeStream *istream;
	GMimeStream *ostream;
	
	GByteArray *diag;
	GMimeStream *diagnostics;
	
	GMimeSigner *signers;
	GMimeSigner *signer;
	
	int exit_status;
	
	unsigned int utf8:1;
	unsigned int exited:1;
	unsigned int complete:1;
	unsigned int seen_eof1:1;
	unsigned int seen_eof2:1;
	unsigned int flushed:1;      /* flushed the diagnostics stream (aka stderr) */
	unsigned int always_trust:1;
	unsigned int armor:1;
	unsigned int need_passwd:1;
	unsigned int send_passwd:1;
	
	unsigned int bad_passwds:2;
	
	unsigned int badsig:1;
	unsigned int errsig:1;
	unsigned int goodsig:1;
	unsigned int validsig:1;
	unsigned int nopubkey:1;
	unsigned int nodata:1;
	
	unsigned int padding:14;
};

static struct _GpgCtx *
gpg_ctx_new (GMimeSession *session, const char *path)
{
	struct _GpgCtx *gpg;
	const char *charset;
	GMimeStream *stream;
	
	gpg = g_slice_new (struct _GpgCtx);
	gpg->mode = GPG_CTX_MODE_SIGN;
	gpg->session = session;
	g_object_ref (session);
	gpg->userid_hint = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	gpg->complete = FALSE;
	gpg->seen_eof1 = TRUE;
	gpg->seen_eof2 = FALSE;
	gpg->pid = (pid_t) -1;
	gpg->exit_status = 0;
	gpg->flushed = FALSE;
	gpg->exited = FALSE;
	
	gpg->path = g_strdup (path);
	gpg->userid = NULL;
	gpg->sigfile = NULL;
	gpg->recipients = NULL;
	gpg->hash = GMIME_CIPHER_HASH_DEFAULT;
	gpg->always_trust = FALSE;
	gpg->armor = FALSE;
	
	gpg->stdin_fd = -1;
	gpg->stdout_fd = -1;
	gpg->stderr_fd = -1;
	gpg->status_fd = -1;
	gpg->passwd_fd = -1;
	
	gpg->statusbuf = g_malloc (128);
	gpg->statusptr = gpg->statusbuf;
	gpg->statusleft = 128;
	
	gpg->bad_passwds = 0;
	gpg->need_passwd = FALSE;
	gpg->send_passwd = FALSE;
	gpg->need_id = NULL;
	gpg->passwd = NULL;
	
	gpg->nodata = FALSE;
	gpg->badsig = FALSE;
	gpg->errsig = FALSE;
	gpg->goodsig = FALSE;
	gpg->validsig = FALSE;
	gpg->nopubkey = FALSE;
	
	gpg->signers = NULL;
	gpg->signer = (GMimeSigner *) &gpg->signers;
	
	gpg->istream = NULL;
	gpg->ostream = NULL;
	
	stream = g_mime_stream_mem_new ();
	gpg->diag = GMIME_STREAM_MEM (stream)->buffer;
	charset = g_mime_locale_charset ();
	if (g_ascii_strcasecmp (charset, "UTF-8") != 0) {
		GMimeStream *fstream;
		GMimeFilter *filter;
		
		fstream = g_mime_stream_filter_new (stream);
		filter = g_mime_filter_charset_new (charset, "UTF-8");
		g_mime_stream_filter_add ((GMimeStreamFilter *) fstream, filter);
		g_object_unref (stream);
		g_object_unref (filter);
		
		gpg->diagnostics = fstream;
		
		gpg->utf8 = FALSE;
	} else {
		/* system charset is UTF-8, shouldn't need any conversion */
		gpg->diagnostics = stream;
		
		gpg->utf8 = TRUE;
	}
	
	return gpg;
}

static void
gpg_ctx_set_mode (struct _GpgCtx *gpg, enum _GpgCtxMode mode)
{
	gpg->mode = mode;
	gpg->need_passwd = ((gpg->mode == GPG_CTX_MODE_SIGN) || (gpg->mode == GPG_CTX_MODE_DECRYPT));
}

static void
gpg_ctx_set_hash (struct _GpgCtx *gpg, GMimeCipherHash hash)
{
	gpg->hash = hash;
}

static void
gpg_ctx_set_always_trust (struct _GpgCtx *gpg, gboolean trust)
{
	gpg->always_trust = trust;
}

static void
gpg_ctx_set_userid (struct _GpgCtx *gpg, const char *userid)
{
	g_free (gpg->userid);
	gpg->userid = g_strdup (userid);
}

static void
gpg_ctx_add_recipient (struct _GpgCtx *gpg, const char *keyid)
{
	if (gpg->mode != GPG_CTX_MODE_ENCRYPT &&
	    gpg->mode != GPG_CTX_MODE_SIGN_ENCRYPT &&
	    gpg->mode != GPG_CTX_MODE_EXPORT)
		return;
	
	if (!gpg->recipients)
		gpg->recipients = g_ptr_array_new ();
	
	g_ptr_array_add (gpg->recipients, g_strdup (keyid));
}

static void
gpg_ctx_set_sigfile (struct _GpgCtx *gpg, const char *sigfile)
{
	g_free (gpg->sigfile);
	gpg->sigfile = g_strdup (sigfile);
}

static void
gpg_ctx_set_armor (struct _GpgCtx *gpg, gboolean armor)
{
	gpg->armor = armor;
}

static void
gpg_ctx_set_istream (struct _GpgCtx *gpg, GMimeStream *istream)
{
	g_object_ref (istream);
	if (gpg->istream)
		g_object_unref (gpg->istream);
	gpg->istream = istream;
}

static void
gpg_ctx_set_ostream (struct _GpgCtx *gpg, GMimeStream *ostream)
{
	g_object_ref (ostream);
	if (gpg->ostream)
		g_object_unref (gpg->ostream);
	gpg->ostream = ostream;
	gpg->seen_eof1 = FALSE;
}

static const char *
gpg_ctx_get_diagnostics (struct _GpgCtx *gpg)
{
	if (!gpg->flushed) {
		g_mime_stream_flush (gpg->diagnostics);
		g_byte_array_append (gpg->diag, (unsigned char *) "", 1);
		gpg->flushed = TRUE;
	}
	
	return (const char *) gpg->diag->data;
}

static void
gpg_ctx_free (struct _GpgCtx *gpg)
{
	GMimeSigner *signer, *next;
	guint i;
	
	if (gpg->session)
		g_object_unref (gpg->session);
	
	g_hash_table_destroy (gpg->userid_hint);
	
	g_free (gpg->path);
	
	g_free (gpg->userid);
	
	g_free (gpg->sigfile);
	
	if (gpg->recipients) {
		for (i = 0; i < gpg->recipients->len; i++)
			g_free (gpg->recipients->pdata[i]);
	
		g_ptr_array_free (gpg->recipients, TRUE);
	}
	
	if (gpg->stdin_fd != -1)
		close (gpg->stdin_fd);
	if (gpg->stdout_fd != -1)
		close (gpg->stdout_fd);
	if (gpg->stderr_fd != -1)
		close (gpg->stderr_fd);
	if (gpg->status_fd != -1)
		close (gpg->status_fd);
	if (gpg->passwd_fd != -1)
		close (gpg->passwd_fd);
	
	g_free (gpg->statusbuf);
	
	g_free (gpg->need_id);
	
	if (gpg->passwd) {
		memset (gpg->passwd, 0, strlen (gpg->passwd));
		g_free (gpg->passwd);
	}
	
	if (gpg->istream)
		g_object_unref (gpg->istream);
	
	if (gpg->ostream)
		g_object_unref (gpg->ostream);
	
	g_object_unref (gpg->diagnostics);
	
	signer = gpg->signers;
	while (signer != NULL) {
		next = signer->next;
		g_mime_signer_free (signer);
		signer = next;
	}
	
	g_slice_free (struct _GpgCtx, gpg);
}

static const char *
gpg_hash_str (GMimeCipherHash hash)
{
	switch (hash) {
	case GMIME_CIPHER_HASH_MD2:
		return "--digest-algo=MD2";
	case GMIME_CIPHER_HASH_MD5:
		return "--digest-algo=MD5";
	case GMIME_CIPHER_HASH_SHA1:
		return "--digest-algo=SHA1";
	case GMIME_CIPHER_HASH_SHA224:
		return "--digest-algo=SHA224";
	case GMIME_CIPHER_HASH_SHA256:
		return "--digest-algo=SHA256";
	case GMIME_CIPHER_HASH_SHA384:
		return "--digest-algo=SHA384";
	case GMIME_CIPHER_HASH_SHA512:
		return "--digest-algo=SHA512";
	case GMIME_CIPHER_HASH_RIPEMD160:
		return "--digest-algo=RIPEMD160";
	case GMIME_CIPHER_HASH_TIGER192:
		return "--digest-algo=TIGER192";
	default:
		return NULL;
	}
}

static GPtrArray *
gpg_ctx_get_argv (struct _GpgCtx *gpg, int status_fd, char **sfd, int passwd_fd, char **pfd)
{
	const char *hash_str;
	GPtrArray *argv;
	char *buf;
	guint i;
	
	argv = g_ptr_array_new ();
	g_ptr_array_add (argv, "gpg");
	
	g_ptr_array_add (argv, "--verbose");
	g_ptr_array_add (argv, "--no-secmem-warning");
	g_ptr_array_add (argv, "--no-greeting");
	g_ptr_array_add (argv, "--no-tty");
	if (passwd_fd == -1) {
		/* only use batch mode if we don't intend on using the
                   interactive --command-fd option */
		g_ptr_array_add (argv, "--batch");
		g_ptr_array_add (argv, "--yes");
	}
	
	g_ptr_array_add (argv, "--charset=UTF-8");
	
	*sfd = buf = g_strdup_printf ("--status-fd=%d", status_fd);
	g_ptr_array_add (argv, buf);
	
	if (passwd_fd != -1) {
		*pfd = buf = g_strdup_printf ("--command-fd=%d", passwd_fd);
		g_ptr_array_add (argv, buf);
	}
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN:
		g_ptr_array_add (argv, "--sign");
		g_ptr_array_add (argv, "--detach");
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		hash_str = gpg_hash_str (gpg->hash);
		if (hash_str)
			g_ptr_array_add (argv, (char *) hash_str);
		if (gpg->userid) {
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (char *) gpg->userid);
		}
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_VERIFY:
		if (!g_mime_session_is_online (gpg->session)) {
			/* this is a deprecated flag to gpg since 1.0.7 */
			/*g_ptr_array_add (argv, "--no-auto-key-retrieve");*/
			g_ptr_array_add (argv, "--keyserver-options");
			g_ptr_array_add (argv, "no-auto-key-retrieve");
		}
		g_ptr_array_add (argv, "--verify");
		if (gpg->sigfile)
			g_ptr_array_add (argv, gpg->sigfile);
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_SIGN_ENCRYPT:
		g_ptr_array_add (argv,  "--sign");
		
		/* fall thru... */
	case GPG_CTX_MODE_ENCRYPT:
		g_ptr_array_add (argv,  "--encrypt");
		
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		
		if (gpg->always_trust)
			g_ptr_array_add (argv, "--always-trust");
		
		if (gpg->userid) {
			g_ptr_array_add (argv, "-u");
			g_ptr_array_add (argv, (char *) gpg->userid);
		}
		
		if (gpg->recipients) {
			for (i = 0; i < gpg->recipients->len; i++) {
				g_ptr_array_add (argv, "-r");
				g_ptr_array_add (argv, gpg->recipients->pdata[i]);
			}
		}
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_DECRYPT:
		g_ptr_array_add (argv, "--decrypt");
		g_ptr_array_add (argv, "--output");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_IMPORT:
		g_ptr_array_add (argv, "--import");
		g_ptr_array_add (argv, "-");
		break;
	case GPG_CTX_MODE_EXPORT:
		if (gpg->armor)
			g_ptr_array_add (argv, "--armor");
		g_ptr_array_add (argv, "--export");
		for (i = 0; i < gpg->recipients->len; i++)
			g_ptr_array_add (argv, gpg->recipients->pdata[i]);
		break;
	}
	
#if d(!)0
	for (i = 0; i < argv->len; i++)
		printf ("%s ", (char *) argv->pdata[i]);
	printf ("\n");
#endif
	
	g_ptr_array_add (argv, NULL);
	
	return argv;
}

static int
gpg_ctx_op_start (struct _GpgCtx *gpg)
{
	char *status_fd = NULL, *passwd_fd = NULL;
	int i, maxfd, errnosave, fds[10];
	GPtrArray *argv;
	int flags;
	
	for (i = 0; i < 10; i++)
		fds[i] = -1;
	
	maxfd = gpg->need_passwd ? 10 : 8;
	for (i = 0; i < maxfd; i += 2) {
		if (pipe (fds + i) == -1)
			goto exception;
	}
	
	argv = gpg_ctx_get_argv (gpg, fds[7], &status_fd, fds[8], &passwd_fd);
	
	if (!(gpg->pid = fork ())) {
		/* child process */
		
		if ((dup2 (fds[0], STDIN_FILENO) < 0 ) ||
		    (dup2 (fds[3], STDOUT_FILENO) < 0 ) ||
		    (dup2 (fds[5], STDERR_FILENO) < 0 )) {
			_exit (255);
		}
		
		/* Dissociate from gmime's controlling terminal so
		 * that gpg won't be able to read from it.
		 */
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		for (i = 3; i < maxfd; i++) {
			/* don't close the status-fd or the passwd-fd */
			if (i != fds[7] && i != fds[8])
				fcntl (i, F_SETFD, FD_CLOEXEC);
		}
		
		/* run gpg */
		execvp (gpg->path, (char **) argv->pdata);
		_exit (255);
	} else if (gpg->pid < 0) {
		g_ptr_array_free (argv, TRUE);
		g_free (status_fd);
		g_free (passwd_fd);
		goto exception;
	}
	
	g_ptr_array_free (argv, TRUE);
	g_free (status_fd);
	g_free (passwd_fd);
	
	/* Parent */
	close (fds[0]);
	gpg->stdin_fd = fds[1];
	gpg->stdout_fd = fds[2];
	close (fds[3]);
	gpg->stderr_fd = fds[4];
	close (fds[5]);
	gpg->status_fd = fds[6];
	close (fds[7]);
	if (gpg->need_passwd) {
		close (fds[8]);
		gpg->passwd_fd = fds[9];
		flags = (flags = fcntl (gpg->passwd_fd, F_GETFL)) == -1 ? O_WRONLY : flags;
		fcntl (gpg->passwd_fd, F_SETFL, flags | O_NONBLOCK);
	}
	
	flags = (flags = fcntl (gpg->stdin_fd, F_GETFL)) == -1 ? O_WRONLY : flags;
	fcntl (gpg->stdin_fd, F_SETFL, flags | O_NONBLOCK);
	
	flags = (flags = fcntl (gpg->stdout_fd, F_GETFL)) == -1 ? O_RDONLY : flags;
	fcntl (gpg->stdout_fd, F_SETFL, flags | O_NONBLOCK);
	
	flags = (flags = fcntl (gpg->stderr_fd, F_GETFL)) == -1 ? O_RDONLY : flags;
	fcntl (gpg->stderr_fd, F_SETFL, flags | O_NONBLOCK);
	
	flags = (flags = fcntl (gpg->status_fd, F_GETFL)) == -1 ? O_RDONLY : flags;
	fcntl (gpg->status_fd, F_SETFL, flags | O_NONBLOCK);
	
	return 0;
	
 exception:
	
	errnosave = errno;
	
	for (i = 0; i < 10; i++) {
		if (fds[i] != -1)
			close (fds[i]);
	}
	
	errno = errnosave;
	
	return -1;
}

static char *
next_token (char *in, char **token)
{
	char *start, *inptr = in;
	
	while (*inptr == ' ')
		inptr++;
	
	if (*inptr == '\0' || *inptr == '\n') {
		if (token)
			*token = NULL;
		return inptr;
	}
	
	start = inptr;
	while (*inptr && *inptr != ' ' && *inptr != '\n')
		inptr++;
	
	if (token)
		*token = g_strndup (start, (size_t) (inptr - start));
	
	return inptr;
}

static void
gpg_ctx_parse_signer_info (struct _GpgCtx *gpg, char *status)
{
	GMimeSigner *signer;
	
	if (!strncmp (status, "SIG_ID ", 7)) {
		/* not sure if this contains anything we care about... */
	} else if (!strncmp (status, "GOODSIG ", 8)) {
		gpg->goodsig = TRUE;
		status += 8;
		
		signer = g_mime_signer_new ();
		signer->status = GMIME_SIGNER_STATUS_GOOD;
		gpg->signer->next = signer;
		gpg->signer = signer;
		
		/* get the key id of the signer */
		status = next_token (status, &signer->keyid);
		
		/* the rest of the string is the signer's name */
		signer->name = g_strdup (status);
	} else if (!strncmp (status, "BADSIG ", 7)) {
		gpg->badsig = TRUE;
		status += 7;
		
		signer = g_mime_signer_new ();
		signer->status = GMIME_SIGNER_STATUS_BAD;
		gpg->signer->next = signer;
		gpg->signer = signer;
		
		/* get the key id of the signer */
		status = next_token (status, &signer->keyid);
		
		/* the rest of the string is the signer's name */
		signer->name = g_strdup (status);
	} else if (!strncmp (status, "ERRSIG ", 7)) {
		/* Note: NO_PUBKEY often comes after an ERRSIG */
		gpg->errsig = TRUE;
		status += 7;
		
		signer = g_mime_signer_new ();
		signer->status = GMIME_SIGNER_STATUS_ERROR;
		gpg->signer->next = signer;
		gpg->signer = signer;
		
		/* get the key id of the signer */
		status = next_token (status, &signer->keyid);
		
		/* skip the pubkey_algo */
		status = next_token (status, NULL);
		
		/* skip the digest_algo */
		status = next_token (status, NULL);
		
		/* skip the class */
		status = next_token (status, NULL);
		
		/* get the signature expiration date (or 0 for never) */
		signer->expires = strtoul (status, NULL, 10);
		status = next_token (status, NULL);
		
		/* the last token is the 'rc' which we don't care about */
	} else if (!strncmp (status, "NO_PUBKEY ", 10)) {
		/* the only token is the keyid, but we've already got it */
		gpg->signer->errors |= GMIME_SIGNER_ERROR_NO_PUBKEY;
		gpg->nopubkey = TRUE;
	} else if (!strncmp (status, "EXPSIG", 6)) {
		/* FIXME: see what else we can glean from this... */
		gpg->signer->errors |= GMIME_SIGNER_ERROR_EXPSIG;
	} else if (!strncmp (status, "EXPKEYSIG", 9)) {
		gpg->signer->errors |= GMIME_SIGNER_ERROR_EXPKEYSIG;
	} else if (!strncmp (status, "REVKEYSIG", 9)) {
		gpg->signer->errors |= GMIME_SIGNER_ERROR_REVKEYSIG;
	} else if (!strncmp (status, "VALIDSIG ", 9)) {
		char *inend;
		
		gpg->validsig = TRUE;
		status += 9;
		
		signer = gpg->signer;
		
		/* the first token is the fingerprint */
		status = next_token (status, &signer->fingerprint);
		
		/* the second token is the date the stream was signed YYYY-MM-DD */
		status = next_token (status, NULL);
		
		/* the third token is the signature creation date (or 0 for unknown?) */
		signer->created = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ')
			return;
		
		status = inend + 1;
		
		/* the fourth token is the signature expiration date (or 0 for never) */
		signer->expires = strtoul (status, NULL, 10);
		
		/* ignore the rest... */
	} else if (!strncmp (status, "TRUST_", 6)) {
		status += 6;
		
		signer = gpg->signer;
		if (!strncmp (status, "NEVER", 5)) {
			signer->trust = GMIME_SIGNER_TRUST_NEVER;
		} else if (!strncmp (status, "MARGINAL", 8)) {
			signer->trust = GMIME_SIGNER_TRUST_MARGINAL;
		} else if (!strncmp (status, "FULLY", 5)) {
			signer->trust = GMIME_SIGNER_TRUST_FULLY;
		} else if (!strncmp (status, "ULTIMATE", 8)) {
			signer->trust = GMIME_SIGNER_TRUST_ULTIMATE;
		} else if (!strncmp (status, "UNDEFINED", 9)) {
			signer->trust = GMIME_SIGNER_TRUST_UNDEFINED;
		}
	}
}

static int
gpg_ctx_parse_status (struct _GpgCtx *gpg, GError **err)
{
	size_t nread, nwritten;
	register char *inptr;
	char *status, *tmp;
	int len;
	
 parse:
	
	g_clear_error (err);
	
	inptr = gpg->statusbuf;
	while (inptr < gpg->statusptr && *inptr != '\n')
		inptr++;
	
	if (*inptr != '\n') {
		/* we don't have enough data buffered to parse this status line */
		return 0;
	}
	
	*inptr++ = '\0';
	status = gpg->statusbuf;
	
	d(printf ("status: %s\n", status));
	
	if (strncmp (status, "[GNUPG:] ", 9) != 0) {
		if (!gpg->utf8)
			tmp = g_locale_to_utf8 (status, -1, &nread, &nwritten, NULL);
		else
			tmp = status;
		
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
			     _("Unexpected GnuPG status message encountered:\n\n%s"),
			     tmp);
		
		if (!gpg->utf8)
			g_free (tmp);
		
		return -1;
	}
	
	status += 9;
	
	if (!strncmp (status, "USERID_HINT ", 12)) {
		size_t nread, nwritten;
		char *hint, *user;
		
		status += 12;
		
		status = next_token (status, &hint);
		if (!hint) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Failed to parse gpg userid hint."));
			return -1;
		}
		
		if (g_hash_table_lookup (gpg->userid_hint, hint)) {
			/* we already have this userid hint... */
			g_free (hint);
			goto recycle;
		}
		
		if (gpg->utf8 || !(user = g_locale_to_utf8 (status, -1, &nread, &nwritten, NULL)))
			user = g_strdup (status);
		
		g_strstrip (user);
		
		g_hash_table_insert (gpg->userid_hint, hint, user);
	} else if (!strncmp (status, "NEED_PASSPHRASE ", 16)) {
		char *userid;
		
		status += 16;
		
		status = next_token (status, &userid);
		if (!userid) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Failed to parse gpg passphrase request."));
			return -1;
		}
		
		g_free (gpg->need_id);
		gpg->need_id = userid;
	} else if (!strncmp (status, "NEED_PASSPHRASE_PIN ", 20)) {
		char *userid;
		
		status += 20;
		
		status = next_token (status, &userid);
		if (!userid) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
				     _("Failed to parse gpg passphrase request."));
			return -1;
		}
		
		g_free (gpg->need_id);
		gpg->need_id = userid;
	} else if (!strncmp (status, "GET_HIDDEN ", 11)) {
		char *prompt = NULL;
		char *passwd = NULL;
		const char *name;
		
		status += 11;
		
		if (!(name = g_hash_table_lookup (gpg->userid_hint, gpg->need_id)))
			name = gpg->userid;
		
		if (!strncmp (status, "passphrase.pin.ask", 18)) {
			prompt = g_strdup_printf (_("You need a PIN to unlock the key for your\n"
						    "SmartCard: \"%s\""), name);
		} else if (!strncmp (status, "passphrase.enter", 16)) {
			prompt = g_strdup_printf (_("You need a passphrase to unlock the key for\n"
						    "user: \"%s\""), name);
		} else {
			next_token (status, &prompt);
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
				     _("Unexpected request from GnuPG for `%s'"), prompt);
			g_free (prompt);
			return -1;
		}
		
		if ((passwd = g_mime_session_request_passwd (gpg->session, prompt, TRUE, gpg->need_id, err))) {
			g_free (prompt);
			
			if (!gpg->utf8) {
				char *locale_passwd;
				
				if ((locale_passwd = g_locale_from_utf8 (passwd, -1, &nread, &nwritten, NULL))) {
					memset (passwd, 0, strlen (passwd));
					g_free (passwd);
					passwd = locale_passwd;
				}
			}
			
			gpg->passwd = g_strdup_printf ("%s\n", passwd);
			memset (passwd, 0, strlen (passwd));
			g_free (passwd);
			
			gpg->send_passwd = TRUE;
		} else {
			if (err && *err == NULL)
				g_set_error (err, GMIME_ERROR, ECANCELED, _("Canceled."));
			
			g_free (prompt);
			
			return -1;
		}
	} else if (!strncmp (status, "GOOD_PASSPHRASE", 15)) {
		gpg->bad_passwds = 0;
	} else if (!strncmp (status, "BAD_PASSPHRASE", 14)) {
		gpg->bad_passwds++;
		
		g_mime_session_forget_passwd (gpg->session, gpg->userid, NULL);
		
		if (gpg->bad_passwds == 3) {
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_BAD_PASSWORD,
				     _("Failed to unlock secret key: 3 bad passphrases given."));
			return -1;
		}
	} else if (!strncmp (status, "UNEXPECTED ", 11)) {
		/* this is an error */
		if (!gpg->utf8)
			tmp = g_locale_to_utf8 (status + 11, -1, &nread, &nwritten, NULL);
		else
			tmp = status + 11;
		
		g_set_error (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
			     _("Unexpected response from GnuPG: %s"),
			     tmp);
		
		if (!gpg->utf8)
			g_free (tmp);
		
		return -1;
	} else if (!strncmp (status, "NODATA", 6)) {
		/* this is an error */
		const char *diagnostics;
		
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		if (diagnostics && *diagnostics)
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
				     "%s", diagnostics);
		else
			g_set_error (err, GMIME_ERROR, GMIME_ERROR_GENERAL,
				     _("No data provided"));
		
		gpg->nodata = TRUE;
		
		return -1;
	} else {
		switch (gpg->mode) {
		case GPG_CTX_MODE_SIGN:
			if (strncmp (status, "SIG_CREATED ", 12) != 0)
				break;
			
			status += 12;
			
			/* skip the next single-char token ("D" for detached) */
			status = next_token (status, NULL);
			
			/* skip the public-key algo token */
			status = next_token (status, NULL);
			
			/* this token is the hash algorithm used */
			switch (strtol (status, NULL, 10)) {
			case 1: gpg->hash = GMIME_CIPHER_HASH_MD5; break;
			case 2: gpg->hash = GMIME_CIPHER_HASH_SHA1; break;
			case 3:	gpg->hash = GMIME_CIPHER_HASH_RIPEMD160; break;
			case 5: gpg->hash = GMIME_CIPHER_HASH_MD2; break; /* ? */
			case 6: gpg->hash = GMIME_CIPHER_HASH_TIGER192; break; /* ? */
			case 7: gpg->hash = GMIME_CIPHER_HASH_HAVAL5160; break; /* ? */
			case 8: gpg->hash = GMIME_CIPHER_HASH_SHA256; break;
			case 9: gpg->hash = GMIME_CIPHER_HASH_SHA384; break;
			case 10: gpg->hash = GMIME_CIPHER_HASH_SHA512; break;
			case 11: gpg->hash = GMIME_CIPHER_HASH_SHA224; break;
			default: break;
			}
			break;
		case GPG_CTX_MODE_VERIFY:
			gpg_ctx_parse_signer_info (gpg, status);
			break;
		case GPG_CTX_MODE_SIGN_ENCRYPT:
		case GPG_CTX_MODE_ENCRYPT:
			if (!strncmp (status, "BEGIN_ENCRYPTION", 16)) {
				/* nothing to do... but we know to expect data on stdout soon */
			} else if (!strncmp (status, "END_ENCRYPTION", 14)) {
				/* nothing to do, but we know the end is near? */
			} else if (!strncmp (status, "NO_RECP", 7)) {
				g_set_error (err, GMIME_ERROR, GMIME_ERROR_NO_VALID_RECIPIENTS,
					     _("Failed to encrypt: No valid recipients specified."));
				return -1;
			}
			break;
		case GPG_CTX_MODE_DECRYPT:
			if (!strncmp (status, "BEGIN_DECRYPTION", 16)) {
				/* nothing to do... but we know to expect data on stdout soon */
			} else if (!strncmp (status, "END_DECRYPTION", 14)) {
				/* nothing to do, but we know we're done */
			} else {
				gpg_ctx_parse_signer_info (gpg, status);
			}
			break;
		case GPG_CTX_MODE_IMPORT:
			/* no-op */
			break;
		case GPG_CTX_MODE_EXPORT:
			/* no-op */
			break;
		}
	}
	
 recycle:
	
	/* recycle our statusbuf by moving inptr to the beginning of statusbuf */
	len = gpg->statusptr - inptr;
	memmove (gpg->statusbuf, inptr, len);
	
	len = inptr - gpg->statusbuf;
	gpg->statusleft += len;
	gpg->statusptr -= len;
	
	/* if we have more data, try parsing the next line? */
	if (gpg->statusptr > gpg->statusbuf)
		goto parse;
	
	return 0;
}

#ifdef ALLOC_NEAREST_POW2
static inline size_t
nearest_pow (size_t num)
{
	size_t n;
	
	if (num == 0)
		return 0;
	
	n = num - 1;
#if defined (__GNUC__) && defined (__i386__)
	__asm__("bsrl %1,%0\n\t"
		"jnz 1f\n\t"
		"movl $-1,%0\n"
		"1:" : "=r" (n) : "rm" (n));
	n = (1 << (n + 1));
#else
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
#endif
	
	return n;
}

#define next_alloc_size(n) nearest_pow (n)
#else
static inline size_t
next_alloc_size (size_t n)
{
	return ((n + 63) / 64) * 64;
}
#endif

#define status_backup(gpg, start, len) G_STMT_START {                     \
	if (gpg->statusleft <= len) {                                     \
		size_t slen, soff;                                        \
		                                                          \
		soff = gpg->statusptr - gpg->statusbuf;                   \
		slen = next_alloc_size (soff + len + 1);                  \
		                                                          \
		gpg->statusbuf = g_realloc (gpg->statusbuf, slen);        \
		gpg->statusptr = gpg->statusbuf + soff;                   \
		gpg->statusleft = (slen - 1) - soff;                      \
	}                                                                 \
	                                                                  \
	memcpy (gpg->statusptr, start, len);                              \
	gpg->statusptr += len;                                            \
	gpg->statusleft -= len;                                           \
} G_STMT_END

enum {
	GPG_STDIN_FD,
	GPG_STDOUT_FD,
	GPG_STDERR_FD,
	GPG_STATUS_FD,
	GPG_PASSWD_FD,
	GPG_N_FDS
};

#ifndef HAVE_POLL
struct pollfd {
	int fd;
	short events;
	short revents;
};

#define POLLIN   (1 << 0)
#define POLLPRI  (1 << 1)
#define POLLOUT  (1 << 2)
#define POLLERR  (1 << 3)
#define POLLHUP  (1 << 4)
#define POLLNVAL (1 << 5)

#ifdef HAVE_SELECT
static int
poll (struct pollfd *pfds, nfds_t nfds, int timeout)
{
	fd_set rset, wset, xset;
	struct timeval tv;
	int maxfd = 0;
	int ready;
	nfds_t i;
	
	if (nfds == 0)
		return 0;
	
	/* initialize our select() timeout */
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	
	/* initialize our select() fd sets */
	FD_ZERO (&rset);
	FD_ZERO (&wset);
	FD_ZERO (&xset);
	
	for (i = 0; i < nfds; i++) {
		if (pfds[i].events & POLLIN)
			FD_SET (pfds[i].fd, &rset);
		if (pfds[i].events & POLLOUT)
			FD_SET (pfds[i].fd, &wset);
		if (pfds[i].events & POLLPRI)
			FD_SET (pfds[i].fd, &xset);
		if (pfds[i].fd > maxfd && (pfds[i].events & (POLLIN | POLLOUT | POLLPRI)))
			maxfd = pfds[i].fd;
		pfds[i].revents = 0;
	}
	
	/* poll our fds... */
	if ((ready = select (maxfd + 1, &rset, &wset, &xset, timeout != -1 ? &tv : NULL)) > 0) {
		ready = 0;
		
		for (i = 0; i < nfds; i++) {
			if (FD_ISSET (pfds[i].fd, &rset))
				pfds[i].revents |= POLLIN;
			if (FD_ISSET (pfds[i].fd, &wset))
				pfds[i].revents |= POLLOUT;
			if (FD_ISSET (pfds[i].fd, &xset))
				pfds[i].revents |= POLLPRI;
			
			if (pfds[i].revents != 0)
				ready++;
		}
	}
	
	return ready;
}
#else
static int
poll (struct pollfd *pfds, nfds_t nfds, int timeout)
{
	errno = EIO;
	return -1;
}
#endif /* HAVE_SELECT */
#endif /* ! HAVE_POLL */

static int
gpg_ctx_op_step (struct _GpgCtx *gpg, GError **err)
{
	const char *diagnostics, *mode;
	struct pollfd pfds[GPG_N_FDS];
	int ready, save;
	nfds_t n;
	
	for (n = 0; n < GPG_N_FDS; n++) {
		pfds[n].events = 0;
		pfds[n].fd = -1;
	}
	
	if (!gpg->seen_eof1) {
		pfds[GPG_STDOUT_FD].fd = gpg->stdout_fd;
		pfds[GPG_STDOUT_FD].events = POLLIN;
	}
	
	if (!gpg->seen_eof2) {
		pfds[GPG_STDERR_FD].fd = gpg->stderr_fd;
		pfds[GPG_STDERR_FD].events = POLLIN;
	}
	
	if (!gpg->complete) {
		pfds[GPG_STATUS_FD].fd = gpg->status_fd;
		pfds[GPG_STATUS_FD].events = POLLIN;
	}
	
	pfds[GPG_STDIN_FD].fd = gpg->stdin_fd;
	pfds[GPG_STDIN_FD].events = POLLOUT;
	
	pfds[GPG_PASSWD_FD].fd = gpg->passwd_fd;
	pfds[GPG_PASSWD_FD].events = POLLOUT;
	
	do {
		for (n = 0; n < GPG_N_FDS; n++)
			pfds[n].revents = 0;
		ready = poll (pfds, GPG_N_FDS, 10 * 1000);
	} while (ready == -1 && errno == EINTR);
	
	if (ready == -1) {
		d(printf ("poll() failed: %s\n", g_strerror (errno)));
		goto exception;
	} else if (ready == 0) {
		/* timed out */
		return 0;
	}
	
	/* Test each and every file descriptor to see if it's 'ready',
	   and if so - do what we can with it and then drop through to
	   the next file descriptor and so on until we've done what we
	   can to all of them. If one fails along the way, return
	   -1. */
	
	if (pfds[GPG_STATUS_FD].revents & (POLLIN | POLLHUP)) {
		/* read the status message and decide what to do... */
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("reading from gpg's status-fd...\n"));
		
		do {
			nread = read (gpg->status_fd, buffer, sizeof (buffer));
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN));
		
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			status_backup (gpg, buffer, (size_t) nread);
			if (gpg_ctx_parse_status (gpg, err) == -1)
				return -1;
		} else {
			gpg->complete = TRUE;
		}
	}
	
	if ((pfds[GPG_STDOUT_FD].revents & (POLLIN | POLLHUP)) && gpg->ostream) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("reading gpg's stdout...\n"));
		
		do {
			nread = read (gpg->stdout_fd, buffer, sizeof (buffer));
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN));
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			if (g_mime_stream_write (gpg->ostream, buffer, (size_t) nread) == -1)
				goto exception;
		} else {
			gpg->seen_eof1 = TRUE;
		}
	}
	
	if (pfds[GPG_STDERR_FD].revents & (POLLIN | POLLHUP)) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("reading gpg's stderr...\n"));
		
		do {
			nread = read (gpg->stderr_fd, buffer, sizeof (buffer));
		} while (nread == -1 && (errno == EINTR || errno == EAGAIN));
		if (nread == -1)
			goto exception;
		
		if (nread > 0) {
			g_mime_stream_write (gpg->diagnostics, buffer, nread);
		} else {
			gpg->seen_eof2 = TRUE;
		}
	}
	
	if ((pfds[GPG_PASSWD_FD].revents & (POLLOUT | POLLHUP)) && gpg->need_passwd && gpg->send_passwd) {
		size_t n, nwritten = 0;
		ssize_t w;
		
		d(printf ("sending gpg our passphrase...\n"));
		
		/* send the passphrase to gpg */
		n = strlen (gpg->passwd);
		do {
			do {
				w = write (gpg->passwd_fd, gpg->passwd + nwritten, n - nwritten);
			} while (w == -1 && (errno == EINTR || errno == EAGAIN));
			
			if (w > 0)
				nwritten += w;
		} while (nwritten < n && w != -1);
		
		/* zero and free our passwd buffer */
		memset (gpg->passwd, 0, n);
		g_free (gpg->passwd);
		gpg->passwd = NULL;
		
		if (w == -1)
			goto exception;
		
		gpg->send_passwd = FALSE;
	}
	
	if ((pfds[GPG_STDIN_FD].revents & (POLLOUT | POLLHUP)) && gpg->istream) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("writing to gpg's stdin...\n"));
		
		/* write our stream to gpg's stdin */
		nread = g_mime_stream_read (gpg->istream, buffer, sizeof (buffer));
		if (nread > 0) {
			ssize_t w, nwritten = 0;
			
			do {
				do {
					w = write (gpg->stdin_fd, buffer + nwritten, nread - nwritten);
				} while (w == -1 && (errno == EINTR || errno == EAGAIN));
				
				if (w > 0)
					nwritten += w;
			} while (nwritten < nread && w != -1);
			
			if (w == -1)
				goto exception;
		}
		
		if (g_mime_stream_eos (gpg->istream)) {
			close (gpg->stdin_fd);
			gpg->stdin_fd = -1;
		}
	}
	
	return 0;
	
 exception:
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN:
		mode = "sign";
		break;
	case GPG_CTX_MODE_VERIFY:
		mode = "verify";
		break;
	case GPG_CTX_MODE_SIGN_ENCRYPT:
	case GPG_CTX_MODE_ENCRYPT:
		mode = "encrypt";
		break;
	case GPG_CTX_MODE_DECRYPT:
		mode = "decrypt";
		break;
	case GPG_CTX_MODE_IMPORT:
		mode = "import keys";
		break;
	case GPG_CTX_MODE_EXPORT:
		mode = "export keys";
		break;
	default:
		g_assert_not_reached ();
		mode = NULL;
		break;
	}
	
	save = errno;
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	errno = save;
	
	if (diagnostics && *diagnostics) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to %s via GnuPG: %s\n\n%s"),
			     mode, g_strerror (errno),
			     diagnostics);
	} else {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to %s via GnuPG: %s\n"),
			     mode, g_strerror (errno));
	}
	
	return -1;
}

static gboolean
gpg_ctx_op_complete (struct _GpgCtx *gpg)
{
	return gpg->complete && gpg->seen_eof1 && gpg->seen_eof2;
}

#if 0
static gboolean
gpg_ctx_op_exited (struct _GpgCtx *gpg)
{
	int status;
	
	if (waitpid (gpg->pid, &status, WNOHANG) == gpg->pid) {
		gpg->exit_status = status;
		gpg->exited = TRUE;
		return TRUE;
	}
	
	return FALSE;
}
#endif

static void
gpg_ctx_op_cancel (struct _GpgCtx *gpg)
{
	int status;
	
	if (gpg->exited)
		return;
	
	kill (gpg->pid, SIGTERM);
	sleep (1);
	if (waitpid (gpg->pid, &status, WNOHANG) == 0) {
		/* no more mr nice guy... */
		kill (gpg->pid, SIGKILL);
		sleep (1);
		waitpid (gpg->pid, &status, WNOHANG);
	}
}

static int
gpg_ctx_op_wait (struct _GpgCtx *gpg)
{
	int errnosave, status;
	sigset_t mask, omask;
	pid_t retval;
	
	if (!gpg->exited) {
		sigemptyset (&mask);
		sigaddset (&mask, SIGALRM);
		sigprocmask (SIG_BLOCK, &mask, &omask);
		
		alarm (1);
		retval = waitpid (gpg->pid, &status, 0);
		errnosave = errno;
		alarm (0);
		
		sigprocmask (SIG_SETMASK, &omask, NULL);
		errno = errnosave;
		
		if (retval == (pid_t) -1 && errno == EINTR) {
			/* gpg is hanging... */
			kill (gpg->pid, SIGTERM);
			sleep (1);
			
			retval = waitpid (gpg->pid, &status, WNOHANG);
			if (retval == (pid_t) 0) {
				/* still hanging... */
				kill (gpg->pid, SIGKILL);
				sleep (1);
				retval = waitpid (gpg->pid, &status, WNOHANG);
			}
		}
	} else {
		status = gpg->exit_status;
		retval = gpg->pid;
	}
	
	if (retval != (pid_t) -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}


static int
gpg_sign (GMimeCipherContext *context, const char *userid, GMimeCipherHash hash,
	  GMimeStream *istream, GMimeStream *ostream, GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_SIGN);
	gpg_ctx_set_hash (gpg, hash);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to execute gpg: %s"),
			     errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, err) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		int save;
		
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	/* save the hash used */
	hash = gpg->hash;
	
	gpg_ctx_free (gpg);
	
	return hash;
}


static char *
swrite (GMimeStream *istream)
{
	GMimeStream *ostream;
	char *template;
	int fd, ret;
	
	template = g_build_filename (g_get_tmp_dir (), "gmime-pgp.XXXXXX", NULL);
	if ((fd = mkstemp (template)) == -1) {
		g_free (template);
		return NULL;
	}
	
	ostream = g_mime_stream_fs_new (fd);
	if ((ret = g_mime_stream_write_to_stream (istream, ostream)) != -1) {
		if ((ret = g_mime_stream_flush (ostream)) != -1)
			ret = g_mime_stream_close (ostream);
	}
	g_object_unref (ostream);
	
	if (ret == -1) {
		unlink (template);
		g_free (template);
		return NULL;
	}
	
	return template;
}

static GMimeSignatureValidity *
gpg_verify (GMimeCipherContext *context, GMimeCipherHash hash,
	    GMimeStream *istream, GMimeStream *sigstream,
	    GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	GMimeSignatureValidity *validity;
	const char *diagnostics;
	struct _GpgCtx *gpg;
	char *sigfile = NULL;
	gboolean valid;
	
	if (sigstream != NULL) {
		/* We are going to verify a detached signature so save
		   the signature to a temp file. */
		if (!(sigfile = swrite (sigstream))) {
			g_set_error (err, GMIME_ERROR, errno,
				     _("Cannot verify message signature: "
				       "could not create temp file: %s"),
				     g_strerror (errno));
			return NULL;
		}
	}
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_VERIFY);
	gpg_ctx_set_hash (gpg, hash);
	gpg_ctx_set_sigfile (gpg, sigfile);
	gpg_ctx_set_istream (gpg, istream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to execute gpg: %s"),
			     errno ? g_strerror (errno) : _("Unknown"));
		goto exception;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, err) == -1) {
			gpg_ctx_op_cancel (gpg);
			goto exception;
		}
	}
	
	valid = gpg_ctx_op_wait (gpg) == 0;
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	
	validity = g_mime_signature_validity_new ();
	g_mime_signature_validity_set_details (validity, diagnostics);
	
	if (gpg->goodsig && !(gpg->badsig || gpg->errsig || gpg->nodata)) {
		/* all signatures were good */
		validity->status = GMIME_SIGNATURE_STATUS_GOOD;
	} else if (gpg->badsig && !(gpg->goodsig && !gpg->errsig)) {
		/* all signatures were bad */
		validity->status = GMIME_SIGNATURE_STATUS_BAD;
	} else if (!gpg->nodata) {
		validity->status = GMIME_SIGNATURE_STATUS_UNKNOWN;
	} else {
		validity->status = GMIME_SIGNATURE_STATUS_BAD;
	}
	
	validity->signers = gpg->signers;
	gpg->signers = NULL;
	
	gpg_ctx_free (gpg);
	
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	return validity;
	
 exception:
	
	gpg_ctx_free (gpg);
	
	if (sigfile) {
		unlink (sigfile);
		g_free (sigfile);
	}
	
	return NULL;
}


static int
gpg_encrypt (GMimeCipherContext *context, gboolean sign, const char *userid,
	     GPtrArray *recipients, GMimeStream *istream, GMimeStream *ostream,
	     GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	guint i;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	if (sign)
		gpg_ctx_set_mode (gpg, GPG_CTX_MODE_SIGN_ENCRYPT);
	else
		gpg_ctx_set_mode (gpg, GPG_CTX_MODE_ENCRYPT);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	gpg_ctx_set_always_trust (gpg, ctx->always_trust);
	
	for (i = 0; i < recipients->len; i++)
		gpg_ctx_add_recipient (gpg, recipients->pdata[i]);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to execute gpg: %s"),
			     errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, err) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		int save;
		
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}


static GMimeSignatureValidity *
gpg_decrypt (GMimeCipherContext *context, GMimeStream *istream,
	     GMimeStream *ostream, GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	GMimeSignatureValidity *validity;
	const char *diagnostics;
	struct _GpgCtx *gpg;
	int save;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_DECRYPT);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to execute gpg: %s"),
			     errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return NULL;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, err) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return NULL;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return NULL;
	}
	
	diagnostics = gpg_ctx_get_diagnostics (gpg);
	
	validity = g_mime_signature_validity_new ();
	g_mime_signature_validity_set_details (validity, diagnostics);
	
	if (gpg->signers) {
		if (gpg->goodsig && !(gpg->badsig || gpg->errsig || gpg->nodata)) {
			/* all signatures were good */
			validity->status = GMIME_SIGNATURE_STATUS_GOOD;
		} else if (gpg->badsig && !(gpg->goodsig && !gpg->errsig)) {
			/* all signatures were bad */
			validity->status = GMIME_SIGNATURE_STATUS_BAD;
		} else if (!gpg->nodata) {
			validity->status = GMIME_SIGNATURE_STATUS_UNKNOWN;
		} else {
			validity->status = GMIME_SIGNATURE_STATUS_BAD;
		}
		
		validity->signers = gpg->signers;
		gpg->signers = NULL;
	}
	
	gpg_ctx_free (gpg);
	
	return validity;
}

static int
gpg_import_keys (GMimeCipherContext *context, GMimeStream *istream, GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_IMPORT);
	gpg_ctx_set_istream (gpg, istream);
	
	if (gpg_ctx_op_start (gpg) == -1) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to execute gpg: %s"),
			     errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, err) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		int save;
		
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}

static int
gpg_export_keys (GMimeCipherContext *context, GPtrArray *keys, GMimeStream *ostream, GError **err)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	guint i;
	
	gpg = gpg_ctx_new (context->session, ctx->path);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_EXPORT);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_ostream (gpg, ostream);
	
	for (i = 0; i < keys->len; i++) {
		gpg_ctx_add_recipient (gpg, keys->pdata[i]);
	}
	
	if (gpg_ctx_op_start (gpg) == -1) {
		g_set_error (err, GMIME_ERROR, errno,
			     _("Failed to execute gpg: %s"),
			     errno ? g_strerror (errno) : _("Unknown"));
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	while (!gpg_ctx_op_complete (gpg)) {
		if (gpg_ctx_op_step (gpg, err) == -1) {
			gpg_ctx_op_cancel (gpg);
			gpg_ctx_free (gpg);
			
			return -1;
		}
	}
	
	if (gpg_ctx_op_wait (gpg) != 0) {
		const char *diagnostics;
		int save;
		
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
}


/**
 * g_mime_gpg_context_new:
 * @session: a #GMimeSession
 * @path: path to gpg binary
 *
 * Creates a new gpg cipher context object.
 *
 * Returns: a new gpg cipher context object.
 **/
GMimeCipherContext *
g_mime_gpg_context_new (GMimeSession *session, const char *path)
{
	GMimeCipherContext *cipher;
	GMimeGpgContext *ctx;
	
	g_return_val_if_fail (GMIME_IS_SESSION (session), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	
	ctx = g_object_newv (GMIME_TYPE_GPG_CONTEXT, 0, NULL);
	ctx->path = g_strdup (path);
	
	cipher = (GMimeCipherContext *) ctx;
	cipher->session = session;
	g_object_ref (session);
	
	return cipher;
}


/**
 * g_mime_gpg_context_get_always_trust:
 * @ctx: a #GMimeGpgContext
 *
 * Gets the @always_trust flag on the gpg context.
 *
 * Returns: the @always_trust flag on the gpg context.
 **/
gboolean
g_mime_gpg_context_get_always_trust (GMimeGpgContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_GPG_CONTEXT (ctx), FALSE);
	
	return ctx->always_trust;
}


/**
 * g_mime_gpg_context_set_always_trust:
 * @ctx: a #GMimeGpgContext
 * @always_trust: always truct flag
 *
 * Sets the @always_trust flag on the gpg context which is used for
 * encryption.
 **/
void
g_mime_gpg_context_set_always_trust (GMimeGpgContext *ctx, gboolean always_trust)
{
	g_return_if_fail (GMIME_IS_GPG_CONTEXT (ctx));
	
	ctx->always_trust = always_trust;
}
