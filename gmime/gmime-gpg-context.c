/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
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

#ifdef __APPLE__
#undef HAVE_POLL_H
#undef HAVE_POLL
typedef unsigned int nfds_t;
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
#ifdef ENABLE_CRYPTOGRAPHY
#include "gmime-filter-charset.h"
#include "gmime-stream-filter.h"
#include "gmime-stream-pipe.h"
#include "gmime-stream-mem.h"
#include "gmime-stream-fs.h"
#include "gmime-charset.h"
#endif /* ENABLE_CRYPTOGRAPHY */
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
 * @short_description: GnuPG crypto contexts
 * @see_also: #GMimeCryptoContext
 *
 * A #GMimeGpgContext is a #GMimeCryptoContext that uses GnuPG to do
 * all of the encryption and digital signatures.
 **/


static void g_mime_gpg_context_class_init (GMimeGpgContextClass *klass);
static void g_mime_gpg_context_init (GMimeGpgContext *ctx, GMimeGpgContextClass *klass);
static void g_mime_gpg_context_finalize (GObject *object);

static GMimeDigestAlgo gpg_digest_id (GMimeCryptoContext *ctx, const char *name);

static const char *gpg_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest);

static int gpg_sign (GMimeCryptoContext *ctx, const char *userid,
		     GMimeDigestAlgo digest, GMimeStream *istream,
		     GMimeStream *ostream, GError **err);

static const char *gpg_get_signature_protocol (GMimeCryptoContext *ctx);

static const char *gpg_get_encryption_protocol (GMimeCryptoContext *ctx);

static const char *gpg_get_key_exchange_protocol (GMimeCryptoContext *ctx);

static GMimeSignatureList *gpg_verify (GMimeCryptoContext *ctx, GMimeDigestAlgo digest,
				       GMimeStream *istream, GMimeStream *sigstream,
				       GError **err);

static int gpg_encrypt (GMimeCryptoContext *ctx, gboolean sign, const char *userid,
			GMimeDigestAlgo digest, GPtrArray *recipients, GMimeStream *istream,
			GMimeStream *ostream, GError **err);

static GMimeDecryptResult *gpg_decrypt (GMimeCryptoContext *ctx, GMimeStream *istream,
					GMimeStream *ostream, GError **err);

static int gpg_import_keys (GMimeCryptoContext *ctx, GMimeStream *istream,
			    GError **err);

static int gpg_export_keys (GMimeCryptoContext *ctx, GPtrArray *keys,
			    GMimeStream *ostream, GError **err);


static GMimeCryptoContextClass *parent_class = NULL;


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
		
		type = g_type_register_static (GMIME_TYPE_CRYPTO_CONTEXT, "GMimeGpgContext", &info, 0);
	}
	
	return type;
}


static void
g_mime_gpg_context_class_init (GMimeGpgContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GMimeCryptoContextClass *crypto_class = GMIME_CRYPTO_CONTEXT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_gpg_context_finalize;
	
	crypto_class->digest_id = gpg_digest_id;
	crypto_class->digest_name = gpg_digest_name;
	crypto_class->sign = gpg_sign;
	crypto_class->verify = gpg_verify;
	crypto_class->encrypt = gpg_encrypt;
	crypto_class->decrypt = gpg_decrypt;
	crypto_class->import_keys = gpg_import_keys;
	crypto_class->export_keys = gpg_export_keys;
	crypto_class->get_signature_protocol = gpg_get_signature_protocol;
	crypto_class->get_encryption_protocol = gpg_get_encryption_protocol;
	crypto_class->get_key_exchange_protocol = gpg_get_key_exchange_protocol;
}

static void
g_mime_gpg_context_init (GMimeGpgContext *ctx, GMimeGpgContextClass *klass)
{
	ctx->auto_key_retrieve = FALSE;
	ctx->always_trust = FALSE;
	ctx->use_agent = FALSE;
	ctx->path = NULL;
}

static void
g_mime_gpg_context_finalize (GObject *object)
{
	GMimeGpgContext *ctx = (GMimeGpgContext *) object;
	
	g_free (ctx->path);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GMimeDigestAlgo
gpg_digest_id (GMimeCryptoContext *ctx, const char *name)
{
	if (name == NULL)
		return GMIME_DIGEST_ALGO_DEFAULT;
	
	if (!g_ascii_strcasecmp (name, "pgp-"))
		name += 4;
	
	if (!g_ascii_strcasecmp (name, "md2"))
		return GMIME_DIGEST_ALGO_MD2;
	else if (!g_ascii_strcasecmp (name, "md4"))
		return GMIME_DIGEST_ALGO_MD4;
	else if (!g_ascii_strcasecmp (name, "md5"))
		return GMIME_DIGEST_ALGO_MD5;
	else if (!g_ascii_strcasecmp (name, "sha1"))
		return GMIME_DIGEST_ALGO_SHA1;
	else if (!g_ascii_strcasecmp (name, "sha224"))
		return GMIME_DIGEST_ALGO_SHA224;
	else if (!g_ascii_strcasecmp (name, "sha256"))
		return GMIME_DIGEST_ALGO_SHA256;
	else if (!g_ascii_strcasecmp (name, "sha384"))
		return GMIME_DIGEST_ALGO_SHA384;
	else if (!g_ascii_strcasecmp (name, "sha512"))
		return GMIME_DIGEST_ALGO_SHA512;
	else if (!g_ascii_strcasecmp (name, "ripemd160"))
		return GMIME_DIGEST_ALGO_RIPEMD160;
	else if (!g_ascii_strcasecmp (name, "tiger192"))
		return GMIME_DIGEST_ALGO_TIGER192;
	else if (!g_ascii_strcasecmp (name, "haval-5-160"))
		return GMIME_DIGEST_ALGO_HAVAL5160;
	
	return GMIME_DIGEST_ALGO_DEFAULT;
}

static const char *
gpg_digest_name (GMimeCryptoContext *ctx, GMimeDigestAlgo digest)
{
	switch (digest) {
	case GMIME_DIGEST_ALGO_MD2:
		return "pgp-md2";
	case GMIME_DIGEST_ALGO_MD4:
		return "pgp-md4";
	case GMIME_DIGEST_ALGO_MD5:
		return "pgp-md5";
	case GMIME_DIGEST_ALGO_SHA1:
		return "pgp-sha1";
	case GMIME_DIGEST_ALGO_SHA224:
		return "pgp-sha224";
	case GMIME_DIGEST_ALGO_SHA256:
		return "pgp-sha256";
	case GMIME_DIGEST_ALGO_SHA384:
		return "pgp-sha384";
	case GMIME_DIGEST_ALGO_SHA512:
		return "pgp-sha512";
	case GMIME_DIGEST_ALGO_RIPEMD160:
		return "pgp-ripemd160";
	case GMIME_DIGEST_ALGO_TIGER192:
		return "pgp-tiger192";
	case GMIME_DIGEST_ALGO_HAVAL5160:
		return "pgp-haval-5-160";
	default:
		return "pgp-sha1";
	}
}

static const char *
gpg_get_signature_protocol (GMimeCryptoContext *ctx)
{
	return "application/pgp-signature";
}

static const char *
gpg_get_encryption_protocol (GMimeCryptoContext *ctx)
{
	return "application/pgp-encrypted";
}

static const char *
gpg_get_key_exchange_protocol (GMimeCryptoContext *ctx)
{
	return "application/pgp-keys";
}

#ifdef ENABLE_CRYPTOGRAPHY
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
	GHashTable *userid_hint;
	GMimeGpgContext *ctx;
	pid_t pid;
	
	char *userid;
	GPtrArray *recipients;
	GMimeCipherAlgo cipher;
	GMimeDigestAlgo digest;
	
	int stdin_fd;
	int stdout_fd;
	int stderr_fd;
	int status_fd;
	int secret_fd;  /* used for sign/decrypt/verify */
	
	/* status-fd buffer */
	char *statusbuf;
	char *statusptr;
	guint statusleft;
	
	char *need_id;
	
	GMimeStream *sigstream;
	GMimeStream *istream;
	GMimeStream *ostream;
	
	GByteArray *diag;
	GMimeStream *diagnostics;
	
	GMimeCertificateList *encrypted_to;  /* full list of encrypted-to recipients */
	GMimeSignatureList *signatures;
	GMimeSignature *signature;
	
	int exit_status;
	
	unsigned int utf8:1;
	unsigned int exited:1;
	unsigned int complete:1;
	unsigned int seen_eof1:1;
	unsigned int seen_eof2:1;
	unsigned int flushed:1;      /* flushed the diagnostics stream (aka stderr) */
	unsigned int always_trust:1;
	unsigned int use_agent:1;
	unsigned int armor:1;
	unsigned int need_passwd:1;
	unsigned int bad_passwds:2;
	unsigned int decrypt_okay:1;
	
	unsigned int padding:19;
};

static struct _GpgCtx *
gpg_ctx_new (GMimeGpgContext *ctx)
{
	struct _GpgCtx *gpg;
	const char *charset;
	GMimeStream *stream;
	
	gpg = g_slice_new (struct _GpgCtx);
	gpg->mode = GPG_CTX_MODE_SIGN;
	gpg->ctx = ctx;
	gpg->userid_hint = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	gpg->decrypt_okay = FALSE;
	gpg->complete = FALSE;
	gpg->seen_eof1 = TRUE;
	gpg->seen_eof2 = FALSE;
	gpg->pid = (pid_t) -1;
	gpg->exit_status = 0;
	gpg->flushed = FALSE;
	gpg->exited = FALSE;
	
	gpg->userid = NULL;
	gpg->recipients = NULL;
	gpg->cipher = GMIME_CIPHER_ALGO_DEFAULT;
	gpg->digest = GMIME_DIGEST_ALGO_DEFAULT;
	gpg->always_trust = FALSE;
	gpg->use_agent = FALSE;
	gpg->armor = FALSE;
	
	gpg->stdin_fd = -1;
	gpg->stdout_fd = -1;
	gpg->stderr_fd = -1;
	gpg->status_fd = -1;
	gpg->secret_fd = -1;
	
	gpg->statusbuf = g_malloc (128);
	gpg->statusptr = gpg->statusbuf;
	gpg->statusleft = 128;
	
	gpg->bad_passwds = 0;
	gpg->need_passwd = FALSE;
	gpg->need_id = NULL;
	
	gpg->encrypted_to = NULL;
	gpg->signatures = NULL;
	gpg->signature = NULL;
	
	gpg->sigstream = NULL;
	gpg->istream = NULL;
	gpg->ostream = NULL;
	
	stream = g_mime_stream_mem_new ();
	gpg->diag = GMIME_STREAM_MEM (stream)->buffer;
	charset = g_mime_charset_iconv_name (g_mime_locale_charset ());
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
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN_ENCRYPT:
	case GPG_CTX_MODE_DECRYPT:
	case GPG_CTX_MODE_SIGN:
		gpg->need_passwd = TRUE;
		break;
	default:
		gpg->need_passwd = FALSE;
		break;
	}
}

static void
gpg_ctx_set_digest (struct _GpgCtx *gpg, GMimeDigestAlgo digest)
{
	gpg->digest = digest;
}

static void
gpg_ctx_set_always_trust (struct _GpgCtx *gpg, gboolean trust)
{
	gpg->always_trust = trust;
}

static void
gpg_ctx_set_use_agent (struct _GpgCtx *gpg, gboolean use_agent)
{
	gpg->use_agent = use_agent;
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

static void
gpg_ctx_set_sigstream (struct _GpgCtx *gpg, GMimeStream *sigstream)
{
	g_object_ref (sigstream);
	if (gpg->sigstream)
		g_object_unref (gpg->sigstream);
	gpg->sigstream = sigstream;
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
	guint i;
	
	g_hash_table_destroy (gpg->userid_hint);
	
	g_free (gpg->userid);
	
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
	if (gpg->secret_fd != -1)
		close (gpg->secret_fd);
	
	g_free (gpg->statusbuf);
	
	g_free (gpg->need_id);
	
	if (gpg->sigstream)
		g_object_unref (gpg->sigstream);
	
	if (gpg->istream)
		g_object_unref (gpg->istream);
	
	if (gpg->ostream)
		g_object_unref (gpg->ostream);
	
	g_object_unref (gpg->diagnostics);
	
	if (gpg->encrypted_to)
		g_object_unref (gpg->encrypted_to);
	
	if (gpg->signatures)
		g_object_unref (gpg->signatures);
	
	g_slice_free (struct _GpgCtx, gpg);
}

static const char *
gpg_digest_str (GMimeDigestAlgo digest)
{
	switch (digest) {
	case GMIME_DIGEST_ALGO_MD2:
		return "--digest-algo=MD2";
	case GMIME_DIGEST_ALGO_MD5:
		return "--digest-algo=MD5";
	case GMIME_DIGEST_ALGO_SHA1:
		return "--digest-algo=SHA1";
	case GMIME_DIGEST_ALGO_SHA224:
		return "--digest-algo=SHA224";
	case GMIME_DIGEST_ALGO_SHA256:
		return "--digest-algo=SHA256";
	case GMIME_DIGEST_ALGO_SHA384:
		return "--digest-algo=SHA384";
	case GMIME_DIGEST_ALGO_SHA512:
		return "--digest-algo=SHA512";
	case GMIME_DIGEST_ALGO_RIPEMD160:
		return "--digest-algo=RIPEMD160";
	case GMIME_DIGEST_ALGO_TIGER192:
		return "--digest-algo=TIGER192";
	case GMIME_DIGEST_ALGO_MD4:
		return "--digest-algo=MD4";
	default:
		return NULL;
	}
}

static char **
gpg_ctx_get_argv (struct _GpgCtx *gpg, int status_fd, int secret_fd, char ***strv)
{
	const char *digest_str;
	char **argv, *buf;
	GPtrArray *args;
	int v = 0;
	guint i;
	
	*strv = g_new (char *, 3);
	
	args = g_ptr_array_new ();
	g_ptr_array_add (args, "gpg");
	
	g_ptr_array_add (args, "--verbose");
	g_ptr_array_add (args, "--no-secmem-warning");
	g_ptr_array_add (args, "--no-greeting");
	g_ptr_array_add (args, "--no-tty");
	
	if (!gpg->need_passwd) {
		/* only use batch mode if we don't intend on using the
                   interactive --command-fd option to send it the
                   user's password */
		g_ptr_array_add (args, "--batch");
		g_ptr_array_add (args, "--yes");
	}
	
	g_ptr_array_add (args, "--charset=UTF-8");
	
	(*strv)[v++] = buf = g_strdup_printf ("--status-fd=%d", status_fd);
	g_ptr_array_add (args, buf);
	
	if (gpg->need_passwd) {
		(*strv)[v++] = buf = g_strdup_printf ("--command-fd=%d", secret_fd);
		g_ptr_array_add (args, buf);
	}
	
	switch (gpg->mode) {
	case GPG_CTX_MODE_SIGN:
		if (gpg->use_agent)
			g_ptr_array_add (args, "--use-agent");
		
		g_ptr_array_add (args, "--sign");
		g_ptr_array_add (args, "--detach");
		if (gpg->armor)
			g_ptr_array_add (args, "--armor");
		if ((digest_str = gpg_digest_str (gpg->digest)))
			g_ptr_array_add (args, (char *) digest_str);
		if (gpg->userid) {
			g_ptr_array_add (args, "-u");
			g_ptr_array_add (args, (char *) gpg->userid);
		}
		g_ptr_array_add (args, "--output");
		g_ptr_array_add (args, "-");
		break;
	case GPG_CTX_MODE_VERIFY:
		if (!gpg->ctx->auto_key_retrieve) {
			g_ptr_array_add (args, "--keyserver-options");
			g_ptr_array_add (args, "no-auto-key-retrieve");
		}
		
		g_ptr_array_add (args, "--enable-special-filenames");
		g_ptr_array_add (args, "--verify");
		g_ptr_array_add (args, "--");
		
		/* signature stream must come first */
		(*strv)[v++] = buf = g_strdup_printf ("-&%d", secret_fd);
		g_ptr_array_add (args, buf);
		
		/* followed by the content stream (in this case, stdin) */
		g_ptr_array_add (args, "-");
		break;
	case GPG_CTX_MODE_SIGN_ENCRYPT:
		if (gpg->use_agent)
			g_ptr_array_add (args, "--use-agent");
		
		g_ptr_array_add (args, "--sign");
		
		if ((digest_str = gpg_digest_str (gpg->digest)))
			g_ptr_array_add (args, (char *) digest_str);
		
		/* fall thru... */
	case GPG_CTX_MODE_ENCRYPT:
		g_ptr_array_add (args, "--encrypt");
		
		if (gpg->armor)
			g_ptr_array_add (args, "--armor");
		
		if (gpg->always_trust)
			g_ptr_array_add (args, "--always-trust");
		
		if (gpg->userid) {
			g_ptr_array_add (args, "-u");
			g_ptr_array_add (args, (char *) gpg->userid);
		}
		
		if (gpg->recipients) {
			for (i = 0; i < gpg->recipients->len; i++) {
				g_ptr_array_add (args, "-r");
				g_ptr_array_add (args, gpg->recipients->pdata[i]);
			}
		}
		g_ptr_array_add (args, "--output");
		g_ptr_array_add (args, "-");
		break;
	case GPG_CTX_MODE_DECRYPT:
		if (gpg->use_agent)
			g_ptr_array_add (args, "--use-agent");
		
		g_ptr_array_add (args, "--decrypt");
		g_ptr_array_add (args, "--output");
		g_ptr_array_add (args, "-");
		break;
	case GPG_CTX_MODE_IMPORT:
		g_ptr_array_add (args, "--import");
		g_ptr_array_add (args, "-");
		break;
	case GPG_CTX_MODE_EXPORT:
		if (gpg->armor)
			g_ptr_array_add (args, "--armor");
		g_ptr_array_add (args, "--export");
		for (i = 0; i < gpg->recipients->len; i++)
			g_ptr_array_add (args, gpg->recipients->pdata[i]);
		break;
	}
	
#if d(!)0
	for (i = 0; i < args->len; i++)
		printf ("%s ", (char *) args->pdata[i]);
	printf ("\n");
#endif
	
	g_ptr_array_add (args, NULL);
	(*strv)[v] = NULL;
	
	argv = (char **) args->pdata;
	g_ptr_array_free (args, FALSE);
	
	return argv;
}

static int
gpg_ctx_op_start (struct _GpgCtx *gpg)
{
	int i, maxfd, errnosave, fds[10];
	char **argv, **strv = NULL;
	int flags;
	
	for (i = 0; i < 10; i++)
		fds[i] = -1;
	
	maxfd = (gpg->need_passwd || gpg->sigstream) ? 10 : 8;
	for (i = 0; i < maxfd; i += 2) {
		if (pipe (fds + i) == -1)
			goto exception;
	}
	
	argv = gpg_ctx_get_argv (gpg, fds[7], fds[8], &strv);
	
	if (!(gpg->pid = fork ())) {
		/* child process */
		
		if ((dup2 (fds[0], STDIN_FILENO) < 0) ||
		    (dup2 (fds[3], STDOUT_FILENO) < 0) ||
		    (dup2 (fds[5], STDERR_FILENO) < 0)) {
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
		execvp (gpg->ctx->path, argv);
		_exit (255);
	} else if (gpg->pid < 0) {
		g_strfreev (strv);
		g_free (argv);
		goto exception;
	}
	
	/* parent process */
	
	g_strfreev (strv);
	g_free (argv);
	
	close (fds[0]);
	gpg->stdin_fd = fds[1];
	gpg->stdout_fd = fds[2];
	close (fds[3]);
	gpg->stderr_fd = fds[4];
	close (fds[5]);
	gpg->status_fd = fds[6];
	close (fds[7]);
	
	if (fds[8] != -1) {
		flags = (flags = fcntl (fds[9], F_GETFL)) == -1 ? O_WRONLY : flags;
		fcntl (fds[9], F_SETFL, flags | O_NONBLOCK);
		gpg->secret_fd = fds[9];
		close (fds[8]);
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
	
	for (i = 0; i < maxfd; i++) {
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

/**
 * gpg_ctx_add_signature:
 * @gpg: GnuPG context
 * @status: a #GMimeSignatureStatus
 * @info: a string with the signature info
 *
 * Parses GOODSIG, BADSIG, EXPSIG, EXPKEYSIG, and REVKEYSIG status messages
 * into a newly allocated #GMimeSignature and adds it to @gpg's signature list.
 **/
static void
gpg_ctx_add_signature (struct _GpgCtx *gpg, GMimeSignatureStatus status, char *info)
{
	GMimeSignature *sig;
	
	if (!gpg->signatures)
		gpg->signatures = g_mime_signature_list_new ();
	
	gpg->signature = sig = g_mime_signature_new ();
	g_mime_signature_set_status (sig, status);
	g_mime_signature_list_add (gpg->signatures, sig);
	g_object_unref (sig);
	
	/* get the key id of the signer */
	info = next_token (info, &sig->cert->keyid);
	
	/* the rest of the string is the signer's name */
	sig->cert->name = g_strdup (info);
}

static void
gpg_ctx_parse_signer_info (struct _GpgCtx *gpg, char *status)
{
	GMimeSignature *sig;
	char *inend;
	
	if (!strncmp (status, "SIG_ID ", 7)) {
		/* not sure if this contains anything we care about... */
	} else if (!strncmp (status, "GOODSIG ", 8)) {
		gpg_ctx_add_signature (gpg, GMIME_SIGNATURE_STATUS_GOOD, status + 8);
	} else if (!strncmp (status, "BADSIG ", 7)) {
		gpg_ctx_add_signature (gpg, GMIME_SIGNATURE_STATUS_BAD, status + 7);
	} else if (!strncmp (status, "EXPSIG ", 7)) {
		gpg_ctx_add_signature (gpg, GMIME_SIGNATURE_STATUS_ERROR, status + 7);
		gpg->signature->errors |= GMIME_SIGNATURE_ERROR_EXPSIG;
	} else if (!strncmp (status, "EXPKEYSIG ", 10)) {
		gpg_ctx_add_signature (gpg, GMIME_SIGNATURE_STATUS_ERROR, status + 10);
		gpg->signature->errors |= GMIME_SIGNATURE_ERROR_EXPKEYSIG;
	} else if (!strncmp (status, "REVKEYSIG ", 10)) {
		gpg_ctx_add_signature (gpg, GMIME_SIGNATURE_STATUS_ERROR, status + 10);
		gpg->signature->errors |= GMIME_SIGNATURE_ERROR_REVKEYSIG;
	} else if (!strncmp (status, "ERRSIG ", 7)) {
		/* Note: NO_PUBKEY often comes after an ERRSIG */
		status += 7;
		
		if (!gpg->signatures)
			gpg->signatures = g_mime_signature_list_new ();
		
		gpg->signature = sig = g_mime_signature_new ();
		g_mime_signature_set_status (sig, GMIME_SIGNATURE_STATUS_ERROR);
		g_mime_signature_list_add (gpg->signatures, sig);
		g_object_unref (sig);
		
		/* get the key id of the signer */
		status = next_token (status, &sig->cert->keyid);
		
		/* the second token is the public-key algorithm id */
		sig->cert->pubkey_algo = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->cert->pubkey_algo = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the third token is the digest algorithm id */
		sig->cert->digest_algo = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->cert->digest_algo = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the fourth token is the signature class */
		/*sig->sig_class =*/ strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			/*signer->sig_class = 0;*/
			return;
		}
		
		status = inend + 1;
		
		/* the fifth token is the signature expiration date (or 0 for never) */
		sig->expires = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->expires = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the sixth token is the return code */
		switch (strtol (status, NULL, 10)) {
		case 4: sig->errors |= GMIME_SIGNATURE_ERROR_UNSUPP_ALGO; break;
		case 9: sig->errors |= GMIME_SIGNATURE_ERROR_NO_PUBKEY; break;
		default: break;
		}
	} else if (!strncmp (status, "NO_PUBKEY ", 10)) {
		/* the only token is the keyid, but we've already got it */
		gpg->signature->errors |= GMIME_SIGNATURE_ERROR_NO_PUBKEY;
	} else if (!strncmp (status, "VALIDSIG ", 9)) {
		sig = gpg->signature;
		status += 9;
		
		/* the first token is the fingerprint */
		status = next_token (status, &sig->cert->fingerprint);
		
		/* the second token is the date the stream was signed YYYY-MM-DD */
		status = next_token (status, NULL);
		
		/* the third token is the signature creation date (or 0 for unknown?) */
		sig->created = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->created = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the fourth token is the signature expiration date (or 0 for never) */
		sig->expires = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->expires = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the fifth token is the signature version */
		/*sig->sig_ver =*/ strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			/*signer->sig_ver = 0;*/
			return;
		}
		
		status = inend + 1;
		
		/* the sixth token is a reserved numeric value (ignore for now) */
		status = next_token (status, NULL);
		
		/* the seventh token is the public-key algorithm id */
		sig->cert->pubkey_algo = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->cert->pubkey_algo = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the eighth token is the digest algorithm id */
		sig->cert->digest_algo = strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			sig->cert->digest_algo = 0;
			return;
		}
		
		status = inend + 1;
		
		/* the nineth token is the signature class */
		/*sig->sig_class =*/ strtoul (status, &inend, 10);
		if (inend == status || *inend != ' ') {
			/*sig->sig_class = 0;*/
			return;
		}
		
		status = inend + 1;
		
		/* the rest is the primary key fingerprint */
	} else if (!strncmp (status, "TRUST_", 6)) {
		status += 6;
		
		sig = gpg->signature;
		if (!strncmp (status, "NEVER", 5)) {
			sig->cert->trust = GMIME_CERTIFICATE_TRUST_NEVER;
		} else if (!strncmp (status, "MARGINAL", 8)) {
			sig->cert->trust = GMIME_CERTIFICATE_TRUST_MARGINAL;
		} else if (!strncmp (status, "FULLY", 5)) {
			sig->cert->trust = GMIME_CERTIFICATE_TRUST_FULLY;
		} else if (!strncmp (status, "ULTIMATE", 8)) {
			sig->cert->trust = GMIME_CERTIFICATE_TRUST_ULTIMATE;
		} else if (!strncmp (status, "UNDEFINED", 9)) {
			sig->cert->trust = GMIME_CERTIFICATE_TRUST_UNDEFINED;
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
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
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
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
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
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR,
					     _("Failed to parse gpg passphrase request."));
			return -1;
		}
		
		g_free (gpg->need_id);
		gpg->need_id = userid;
	} else if (!strncmp (status, "GET_HIDDEN ", 11)) {
		GMimeStream *filtered_stream, *passwd;
		GMimeCryptoContext *ctx;
		GMimeFilter *filter;
		const char *charset;
		char *prompt = NULL;
		const char *name;
		gboolean ok;
		
		status += 11;
		
		ctx = (GMimeCryptoContext *) gpg->ctx;
		if (!ctx->request_passwd) {
			/* can't ask for a passwd w/o a way to request it from the user... */
			g_set_error_literal (err, GMIME_ERROR, ECANCELED, _("Canceled."));
			return -1;
		}
		
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
		
		/* create a stream for the application to write the passwd to */
		passwd = g_mime_stream_pipe_new (gpg->secret_fd);
		g_mime_stream_pipe_set_owner ((GMimeStreamPipe *) passwd, FALSE);
		
		if (!gpg->utf8) {
			/* we'll need to transcode the UTF-8 password that the application
			 * will write to our stream into the locale charset used by gpg */
			filtered_stream = g_mime_stream_filter_new (passwd);
			g_object_unref (passwd);
			
			charset = g_mime_locale_charset ();
			filter = g_mime_filter_charset_new ("UTF-8", charset);
			
			g_mime_stream_filter_add ((GMimeStreamFilter *) filtered_stream, filter);
			g_object_unref (filter);
			
			passwd = filtered_stream;
		}
		
		if ((ok = ctx->request_passwd (ctx, name, prompt, gpg->bad_passwds > 0, passwd, err))) {
			if (g_mime_stream_flush (passwd) == -1)
				ok = FALSE;
		}
		
		g_object_unref (passwd);
		g_free (prompt);
		
		if (!ok)
			return -1;
	} else if (!strncmp (status, "GOOD_PASSPHRASE", 15)) {
		gpg->bad_passwds = 0;
	} else if (!strncmp (status, "BAD_PASSPHRASE", 14)) {
		gpg->bad_passwds++;
		
		if (gpg->bad_passwds == 3) {
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_BAD_PASSWORD,
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
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_GENERAL, diagnostics);
		else
			g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_GENERAL, _("No data provided"));
		
		return -1;
	} else {
		GMimeCertificate *cert;
		char *inend;
		
		switch (gpg->mode) {
		case GPG_CTX_MODE_SIGN:
			if (strncmp (status, "SIG_CREATED ", 12) != 0)
				break;
			
			status += 12;
			
			/* skip the next single-char token ("D" for detached) */
			status = next_token (status, NULL);
			
			/* skip the public-key algorithm id token */
			status = next_token (status, NULL);
			
			/* this token is the digest algorithm used */
			gpg->digest = strtoul (status, NULL, 10);
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
				g_set_error_literal (err, GMIME_ERROR, GMIME_ERROR_NO_VALID_RECIPIENTS,
						     _("Failed to encrypt: No valid recipients specified."));
				return -1;
			}
			break;
		case GPG_CTX_MODE_DECRYPT:
			if (!strncmp (status, "BEGIN_DECRYPTION", 16)) {
				/* nothing to do... but we know to expect data on stdout soon */
			} else if (!strncmp (status, "DECRYPTION_INFO ", 16)) {
				/* new feature added in gnupg-2.1.x which gives mdc and cipher algorithms used */
				status += 16;
				
				/* first token is the mdc algorithm (or 0 if not used) */
				gpg->digest = strtoul (status, &inend, 10);
				if (inend == status || *inend != ' ') {
					gpg->digest = 0;
					break;
				}
				
				status = inend + 1;
				
				/* second token is the cipher algorithm */
				gpg->cipher = strtoul (status, &inend, 10);
			} else if (!strncmp (status, "DECRYPTION_OKAY", 15)) {
				/* decryption succeeded */
				gpg->decrypt_okay = TRUE;
			} else if (!strncmp (status, "DECRYPTION_FAILED", 17)) {
				/* nothing to do... but we know gpg failed to decrypt :-( */
			} else if (!strncmp (status, "END_DECRYPTION", 14)) {
				/* nothing to do, but we know we're done */
			} else if (!strncmp (status, "ENC_TO ", 7)) {
				/* parse the recipient info */
				if (!gpg->encrypted_to)
					gpg->encrypted_to = g_mime_certificate_list_new ();
				
				cert = g_mime_certificate_new ();
				g_mime_certificate_list_add (gpg->encrypted_to, cert);
				
				status += 7;
				
				/* first token is the recipient's keyid */
				status = next_token (status, &cert->keyid);
				
				/* second token is the recipient's pubkey algo */
				cert->pubkey_algo = strtoul (status, &inend, 10);
				if (inend == status || *inend != ' ') {
					cert->pubkey_algo = 0;
					g_object_unref (cert);
					break;
				}
				
				g_object_unref (cert);
				status = inend + 1;
				
				/* third token is a dummy value which is always '0' */
			} else if (!strncmp (status, "GOODMDC", 7)) {
				/* nothing to do... we'll grab the MDC used in DECRYPTION_INFO */
			} else if (!strncmp (status, "BADMDC", 6)) {
				/* nothing to do, this will only be sent after DECRYPTION_FAILED */
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
	return (n + 63) & ~63;
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
	GPG_VERIFY_FD,
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
		pfds[i].revents = 0;
		if (pfds[i].fd < 0)
			continue;
		
		if (pfds[i].events & POLLIN)
			FD_SET (pfds[i].fd, &rset);
		if (pfds[i].events & POLLOUT)
			FD_SET (pfds[i].fd, &wset);
		if (pfds[i].events != 0)
			FD_SET (pfds[i].fd, &xset);
		if (pfds[i].fd > maxfd)
			maxfd = pfds[i].fd;
	}
	
	/* poll our fds... */
	if ((ready = select (maxfd + 1, &rset, &wset, &xset, timeout != -1 ? &tv : NULL)) > 0) {
		ready = 0;
		
		for (i = 0; i < nfds; i++) {
			if (pfds[i].fd < 0)
				continue;
			
			if (FD_ISSET (pfds[i].fd, &rset))
				pfds[i].revents |= POLLIN;
			if (FD_ISSET (pfds[i].fd, &wset))
				pfds[i].revents |= POLLOUT;
			if (FD_ISSET (pfds[i].fd, &xset))
				pfds[i].revents |= POLLERR | POLLHUP;
			
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
	
	if (gpg->mode == GPG_CTX_MODE_VERIFY) {
		pfds[GPG_VERIFY_FD].fd = gpg->secret_fd;
		pfds[GPG_VERIFY_FD].events = POLLOUT;
	}
	
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
		
		d(printf ("reading gpg's status-fd...\n"));
		
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
	
	if ((pfds[GPG_VERIFY_FD].revents & (POLLOUT | POLLHUP))) {
		char buffer[4096];
		ssize_t nread;
		
		d(printf ("streaming digital signature to gpg...\n"));
		
		/* write our signature stream to gpg's special fd */
		nread = g_mime_stream_read (gpg->sigstream, buffer, sizeof (buffer));
		if (nread > 0) {
			ssize_t w, nwritten = 0;
			
			do {
				do {
					w = write (gpg->secret_fd, buffer + nwritten, nread - nwritten);
				} while (w == -1 && (errno == EINTR || errno == EAGAIN));
				
				if (w > 0)
					nwritten += w;
			} while (nwritten < nread && w != -1);
			
			if (w == -1)
				goto exception;
		}
		
		if (g_mime_stream_eos (gpg->sigstream)) {
			close (gpg->secret_fd);
			gpg->secret_fd = -1;
		}
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
#endif /* ENABLE_CRYPTOGRAPHY */

static int
gpg_sign (GMimeCryptoContext *context, const char *userid, GMimeDigestAlgo digest,
	  GMimeStream *istream, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (ctx);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_SIGN);
	gpg_ctx_set_use_agent (gpg, ctx->use_agent);
	gpg_ctx_set_digest (gpg, digest);
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
		
		g_set_error_literal (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	/* save the digest used */
	digest = gpg->digest;
	
	gpg_ctx_free (gpg);
	
	return digest;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTOGRAPHY */
}


static GMimeSignatureList *
gpg_verify (GMimeCryptoContext *context, GMimeDigestAlgo digest,
	    GMimeStream *istream, GMimeStream *sigstream,
	    GError **err)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	GMimeSignatureList *signatures;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (ctx);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_VERIFY);
	gpg_ctx_set_sigstream (gpg, sigstream);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_digest (gpg, digest);
	
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
	
	/* Only set the GError if we got no signature information from gpg */
	if (gpg_ctx_op_wait (gpg) != 0 && !gpg->signatures) {
		const char *diagnostics;
		int save;
		
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error_literal (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return NULL;
	}
	
	signatures = gpg->signatures;
	gpg->signatures = NULL;
	gpg_ctx_free (gpg);
	
	return signatures;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_CRYPTOGRAPHY */
}


static int
gpg_encrypt (GMimeCryptoContext *context, gboolean sign, const char *userid,
	     GMimeDigestAlgo digest, GPtrArray *recipients, GMimeStream *istream,
	     GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	guint i;
	
	gpg = gpg_ctx_new (ctx);
	if (sign) {
		gpg_ctx_set_mode (gpg, GPG_CTX_MODE_SIGN_ENCRYPT);
		gpg_ctx_set_use_agent (gpg, ctx->use_agent);
	} else {
		gpg_ctx_set_mode (gpg, GPG_CTX_MODE_ENCRYPT);
	}
	
	gpg_ctx_set_always_trust (gpg, ctx->always_trust);
	gpg_ctx_set_digest (gpg, digest);
	gpg_ctx_set_armor (gpg, TRUE);
	gpg_ctx_set_userid (gpg, userid);
	gpg_ctx_set_istream (gpg, istream);
	gpg_ctx_set_ostream (gpg, ostream);
	
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
		
		g_set_error_literal (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTOGRAPHY */
}


static GMimeDecryptResult *
gpg_decrypt (GMimeCryptoContext *context, GMimeStream *istream,
	     GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	GMimeDecryptResult *result;
	const char *diagnostics;
	struct _GpgCtx *gpg;
	int save;
	
	gpg = gpg_ctx_new (ctx);
	gpg_ctx_set_mode (gpg, GPG_CTX_MODE_DECRYPT);
	gpg_ctx_set_use_agent (gpg, ctx->use_agent);
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
	
	if (gpg_ctx_op_wait (gpg) != 0 && !gpg->decrypt_okay) {
		save = errno;
		diagnostics = gpg_ctx_get_diagnostics (gpg);
		errno = save;
		
		g_set_error_literal (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return NULL;
	}
	
	result = g_mime_decrypt_result_new ();
	result->recipients = gpg->encrypted_to;
	result->signatures = gpg->signatures;
	result->cipher = gpg->cipher;
	result->mdc = gpg->digest;
	gpg->encrypted_to = NULL;
	gpg->signatures = NULL;
	
	gpg_ctx_free (gpg);
	
	return result;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return NULL;
#endif /* ENABLE_CRYPTOGRAPHY */
}

static int
gpg_import_keys (GMimeCryptoContext *context, GMimeStream *istream, GError **err)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	
	gpg = gpg_ctx_new (ctx);
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
		
		g_set_error_literal (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTOGRAPHY */
}

static int
gpg_export_keys (GMimeCryptoContext *context, GPtrArray *keys, GMimeStream *ostream, GError **err)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeGpgContext *ctx = (GMimeGpgContext *) context;
	struct _GpgCtx *gpg;
	guint i;
	
	gpg = gpg_ctx_new (ctx);
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
		
		g_set_error_literal (err, GMIME_ERROR, errno, diagnostics);
		gpg_ctx_free (gpg);
		
		return -1;
	}
	
	gpg_ctx_free (gpg);
	
	return 0;
#else
	g_set_error (err, GMIME_ERROR, GMIME_ERROR_NOT_SUPPORTED, _("PGP support is not enabled in this build"));
	
	return -1;
#endif /* ENABLE_CRYPTOGRAPHY */
}


/**
 * g_mime_gpg_context_new:
 * @request_passwd: a #GMimePasswordRequestFunc
 * @path: path to gpg binary
 *
 * Creates a new gpg crypto context object.
 *
 * Returns: (transfer full): a new gpg crypto context object.
 **/
GMimeCryptoContext *
g_mime_gpg_context_new (GMimePasswordRequestFunc request_passwd, const char *path)
{
#ifdef ENABLE_CRYPTOGRAPHY
	GMimeCryptoContext *crypto;
	GMimeGpgContext *ctx;
	
	g_return_val_if_fail (path != NULL, NULL);
	
	ctx = g_object_newv (GMIME_TYPE_GPG_CONTEXT, 0, NULL);
	ctx->path = g_strdup (path);
	
	crypto = (GMimeCryptoContext *) ctx;
	crypto->request_passwd = request_passwd;
	
	return crypto;
#else
	return NULL;
#endif /* ENABLE_CRYPTOGRAPHY */
}


/**
 * g_mime_gpg_context_get_auto_key_retrieve:
 * @ctx: a #GMimeGpgContext
 *
 * Gets the @auto_key_retrieve flag on the gpg context.
 *
 * Returns: the @auto_key_retrieve flag on the gpg context.
 **/
gboolean
g_mime_gpg_context_get_auto_key_retrieve (GMimeGpgContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_GPG_CONTEXT (ctx), FALSE);
	
	return ctx->auto_key_retrieve;
}


/**
 * g_mime_gpg_context_set_auto_key_retrieve:
 * @ctx: a #GMimeGpgContext
 * @auto_key_retrieve: auto-retrieve keys from a keys server
 *
 * Sets the @auto_key_retrieve flag on the gpg context which is used
 * for signature verification.
 **/
void
g_mime_gpg_context_set_auto_key_retrieve (GMimeGpgContext *ctx, gboolean auto_key_retrieve)
{
	g_return_if_fail (GMIME_IS_GPG_CONTEXT (ctx));
	
	ctx->auto_key_retrieve = auto_key_retrieve;
}


/**
 * g_mime_gpg_context_get_always_trust:
 * @ctx: a #GMimeGpgContext
 *
 * Gets the always_trust flag on the gpg context.
 *
 * Returns: the always_trust flag on the gpg context.
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
 * @always_trust: always trust flag
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


/**
 * g_mime_gpg_context_get_use_agent:
 * @ctx: a #GMimeGpgContext
 *
 * Gets the use_agent flag on the gpg context.
 *
 * Returns: the use_agent flag on the gpg context, which indicates
 * that GnuPG should attempt to use gpg-agent for credentials.
 **/
gboolean
g_mime_gpg_context_get_use_agent (GMimeGpgContext *ctx)
{
	g_return_val_if_fail (GMIME_IS_GPG_CONTEXT (ctx), FALSE);
	
	return ctx->use_agent;
}


/**
 * g_mime_gpg_context_set_use_agent:
 * @ctx: a #GMimeGpgContext
 * @use_agent: always trust flag
 *
 * Sets the @use_agent flag on the gpg context, which indicates that
 * GnuPG should attempt to use gpg-agent for credentials.
 **/
void
g_mime_gpg_context_set_use_agent (GMimeGpgContext *ctx, gboolean use_agent)
{
	g_return_if_fail (GMIME_IS_GPG_CONTEXT (ctx));
	
	ctx->use_agent = use_agent;
}
