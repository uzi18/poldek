/*
  Copyright (C) 2003 Pawel A. Gajda <mis@k2.net.pl>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2 as
  published by the Free Software Foundation (see file COPYING for details).

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
  $Id$
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <trurl/nassert.h>
#include <trurl/nstr.h>
#include <trurl/nhash.h>
#include <trurl/narray.h>
#include <trurl/nmalloc.h>

#include "sigint/sigint.h"

#include "i18n.h"

#define VFILE_INTERNAL
#include "vfile.h"




static int extr_port(char *host) 
{
    char *p;
    int port = 0;
    
    if ((p = strrchr(host, ':'))) {
        if (sscanf(p + 1, "%d", &port) != 1)
            return -1;
        
        *p = '\0';
    }

    return port;
}

static
int vf_parse_url(char *url, struct vf_request *req) 
{
    char               *p, *q;

    if ((p = strstr(url, "://")) == NULL) 
        return 0;
    
    req->proto = url;
    *p = '\0';
    p += 3;
    req->host = p;
    
    if ((q = strchr(p, '/')) != NULL) {
        *q = '\0';
        req->uri = q+1;
    }

    /* extract loginname from hostpart */
    if ((p = strrchr(req->host, '@')) != NULL) {
        *p = '\0';
        req->login = req->host;
        req->host = p + 1;

        if ((p = strchr(req->login, ':')) == NULL) 
            return 0;

        *p = '\0';
        req->passwd = p + 1;
    }

    if ((req->port = extr_port(req->host)) < 0) 
        return 0;

    n_assert(req->proto && req->host);
    return 1;
}

static const char *get_proxy(const char *proto)
{
    char *url = NULL, buf[256];

    if ((url = n_hash_get(vfile_conf.proxies_ht, proto)))
        return url;
    
    snprintf(buf, sizeof(buf), "%s_proxy", proto);

    if ((url = getenv(buf)) == NULL || *url == 0) {
        char *p;

        p = buf;
        while (*p) {
            *p = toupper(*p);
            p++;
        }

        url = getenv(buf);
    }

    if (url) {
        if (*url == '\0')
            url = NULL;

        else {
            url = n_strdup(url);
            n_hash_insert(vfile_conf.proxies_ht, proto, url);
        }
    } 

    return url;
}

    
struct vf_request *vf_request_new(const char *url, const char *destpath) 
{
    char               buf[PATH_MAX], tmp[PATH_MAX];
    const char         *proxy = NULL;
    char               *err_msg = _("%s: URL parse error\n");
    struct vf_request  *req, rreq, preq;
    int                len;

    memset(&rreq, 0, sizeof(rreq));

    //printf("**request new %s\n", url);
    snprintf(buf, sizeof(buf), "%s", url);
    
    if (!vf_parse_url(buf, &rreq) || rreq.uri == NULL) {
        vfile_err_fn(err_msg, CL_URL(url));
        return NULL;
    }
    
    
    if ((proxy = get_proxy(rreq.proto))) {
        char pbuf[PATH_MAX];

        snprintf(pbuf, sizeof(pbuf), "%s", proxy);
        memset(&preq, 0, sizeof(preq));
        
        if (!vf_parse_url(pbuf, &preq)) {
            vfile_err_fn(err_msg, CL_URL(proxy));
            return NULL;
        }
    }
        
        
    req = n_malloc(sizeof(*req));
    memset(req, 0, sizeof(*req));
    
    if (destpath)
        req->destpath = n_strdup(destpath);
    else
        req->destpath = NULL;
    req->stream = NULL;
    req->stream_offset = 0;

    len = n_snprintf(tmp, sizeof(tmp), "%s://%s/%s", rreq.proto, rreq.host,
                     rreq.uri);
    req->url = n_strdupl(tmp, len);
    req->proto = n_strdup(rreq.proto);
    req->host = n_strdup(rreq.host);

    len = n_snprintf(tmp, sizeof(tmp), "/%s", rreq.uri);
    req->uri = n_strdupl(tmp, len);

    req->port = rreq.port;
    if (rreq.login)
        req->login = n_strdup(rreq.login);

    if (rreq.passwd)
        req->passwd = n_strdup(rreq.passwd);

    if (proxy && *proxy) {
        len = n_snprintf(tmp, sizeof(tmp), "%s://%s", preq.proto, preq.host);
        req->proxy_url = n_strdupl(tmp, len);
        
        req->proxy_proto = n_strdup(preq.proto);
        req->proxy_host = n_strdup(preq.host);
        if (preq.login)
            req->proxy_login = n_strdup(preq.login);

        if (preq.passwd)
            req->proxy_passwd = n_strdup(preq.passwd);
        req->proxy_port = preq.port;
    }
    
    return req;
}
    
void vf_request_free(struct vf_request *req)
{
    n_cfree(&req->url);
    n_cfree(&req->proto);
    n_cfree(&req->host);
    n_cfree(&req->uri);

    n_cfree(&req->login);
    n_cfree(&req->passwd);

    n_cfree(&req->proxy_url);
    n_cfree(&req->proxy_proto);
    n_cfree(&req->proxy_host);
    n_cfree(&req->proxy_login);
    n_cfree(&req->proxy_passwd);

    n_cfree(&req->destpath);
    if (req->stream) {
        fclose(req->stream);
        req->stream = NULL;
    }

    free(req);
}


#define xx_replace(p, q) n_cfree(&p); p = q; q = NULL; 

struct vf_request *vf_request_redirto(struct vf_request *req, const char *url)
{
    struct vf_request *tmpreq;

    tmpreq = vf_request_new(url, NULL);
    if (tmpreq == NULL)
        return NULL;

    
        
    
    vfile_msg_fn("Redirected to %s\n", PR_URL(tmpreq->url));
    
    xx_replace(req->url, tmpreq->url);

    if (strcmp(req->proto, tmpreq->proto) != 0)
        req->flags |= VF_REQ_INT_REDIRECTED;

    if ((req->flags & VF_REQ_INT_REDIRECTED) ||
        strcmp(req->host, tmpreq->host) != 0) {
        
        xx_replace(req->proto, tmpreq->proto);
        xx_replace(req->host, tmpreq->host);
        n_cfree(&req->login);
        n_cfree(&req->passwd);
    }
    
    xx_replace(req->uri, tmpreq->uri);
    
    xx_replace(req->proxy_proto, tmpreq->proxy_proto);
    xx_replace(req->proxy_host, tmpreq->proxy_host);
    xx_replace(req->proxy_login, tmpreq->proxy_login);
    xx_replace(req->proxy_passwd, tmpreq->proxy_passwd);

    vf_request_free(tmpreq);
    return req;
}

    