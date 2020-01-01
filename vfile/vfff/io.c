/*
  Copyright (C) 2000 - 2008 Pawel A. Gajda <mis@pld-linux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License version 2 as
  published by the Free Software Foundation (see file COPYING for details).

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

//#include <trurl/nbuf.h>
#include <trurl/nassert.h>
#include <trurl/nmalloc.h>

#include "vfff.h"

struct sslmod {
    SSL_CTX *ctx;
    SSL *ssl;
};

static int raw_read(struct vcn *cn, void *buf, size_t n)
{
    return read(cn->sockfd, buf, n);
}

static int raw_write(struct vcn *cn, void *buf, size_t n)
{
    return write(cn->sockfd, buf, n);
}

static int raw_select(struct vcn *cn, unsigned timeout)
{
    struct timeval to = { timeout, 0 };
    fd_set fdset;

    FD_ZERO(&fdset);
    FD_SET(cn->sockfd, &fdset);

    return select(cn->sockfd + 1, &fdset, NULL, NULL, &to);
}

static int ssl_read(struct vcn *cn, void *buf, size_t n)
{
    struct sslmod *mod = cn->iomod;
    n_assert(mod);

    return SSL_read(mod->ssl, buf, n);
}

static int ssl_write(struct vcn *cn, void *buf, size_t n)
{
    struct sslmod *mod = cn->iomod;

    n_assert(mod);
    return SSL_write(mod->ssl, buf, n);
}

static int ssl_select(struct vcn *cn, unsigned timeout)
{
    struct sslmod *mod = cn->iomod;

    n_assert(mod);

    int pending = SSL_pending(mod->ssl);
    if (pending > 0)
        return pending;

    return raw_select(cn, timeout);
}

static struct sslmod *init_ssl(const struct vcn *cn)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    SSL *ssl;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, cn->sockfd);
    if (SSL_connect(ssl) != 1)
        ERR_print_errors_fp(stderr);

    struct sslmod *mod = n_malloc(sizeof(*mod));
    mod->ctx = ctx;
    mod->ssl = ssl;

    return mod;
}

int vfff_io_init(struct vcn *cn) {
    if (cn->proto == VCN_PROTO_HTTPS) {
        cn->iomod = init_ssl(cn);
        cn->io_read = ssl_read;
        cn->io_write = ssl_write;
        cn->io_select = ssl_select;
    } else {
        cn->io_read = raw_read;
        cn->io_write = raw_write;
        cn->io_select = raw_select;
    }

    return 1;
}

void vfff_io_destroy(struct vcn *cn) {
    struct sslmod *mod = cn->iomod;
    if (mod) {
        SSL_free(mod->ssl);
        SSL_CTX_free(mod->ctx);
        free(mod);
        cn->iomod = NULL;
    }

    cn->io_read = NULL;
    cn->io_write = NULL;
    cn->io_select = NULL;
}