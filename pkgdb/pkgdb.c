/* 
  Copyright (C) 2000 Pawel A. Gajda (mis@k2.net.pl)
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License published by
  the Free Software Foundation (see file COPYING for details).
*/

/*
  $Id$
*/

#include <stdlib.h>
#include <string.h>

#include <fcntl.h>              /* for O_* */

#include <trurl/nassert.h>
#include <trurl/nmalloc.h>

#include "i18n.h"
#include "pkgdb.h"
#include "poldek_ts.h"
#include "rpm/rpm.h"

struct pkgdb *pkgdb_open(const char *rootdir, const char *path, mode_t mode)
{
    struct pkgdb *db;

    if ((db = n_malloc(sizeof(*db))) == NULL)
        return NULL;

    if (path == NULL)
        path = RPM_DBPATH;
    
    if ((db->dbh = rpm_opendb(path, rootdir, mode)) == NULL)
        return NULL;

    db->mode = mode;
    db->path = n_strdup(path);
    if (rootdir)
        db->rootdir = n_strdup(rootdir);
    else
        db->rootdir = NULL;
    
    return db;
}

void pkgdb_closedb(struct pkgdb *db) 
{
    if (db->dbh) {
        rpm_closedb(db->dbh);
        db->dbh = NULL;
    }
}

int pkgdb_reopendb(struct pkgdb *db) 
{
    if (db->dbh == NULL) 
        db->dbh = rpm_opendb(db->path, db->rootdir, db->mode);
    return db->dbh != NULL;
}


void pkgdb_free(struct pkgdb *db) 
{
    pkgdb_closedb(db);

    if (db->path) {
        free(db->path);
        db->path = NULL;
    }

    if (db->rootdir) {
        free(db->rootdir);
        db->rootdir = NULL;
    }
    
    free(db);
}


int pkgdb_match_req(struct pkgdb *db, const struct capreq *req, int strict,
                    tn_array *excloffs) 
{
    n_assert(db->dbh);
    return rpm_dbmatch_req(db->dbh, req, strict, excloffs);
}


int pkgdb_install(struct pkgdb *db, const char *path,
                  const struct poldek_ts *ts) 
{
    unsigned instflags = 0, filterflags = 0, transflags = 0;

    n_assert(db->dbh);

    
    if (ts->getop(ts, POLDEK_OP_NODEPS))
        instflags |= INSTALL_NODEPS;
    
    if (ts->getop(ts, POLDEK_OP_JUSTDB))
        transflags |= RPMTRANS_FLAG_JUSTDB;
    
    if (ts->getop(ts, POLDEK_OP_TEST))
        transflags |= RPMTRANS_FLAG_TEST;
    
    if (ts->getop(ts, POLDEK_OP_FORCE))
        filterflags |= RPMPROB_FILTER_REPLACEPKG |
            RPMPROB_FILTER_REPLACEOLDFILES |
            RPMPROB_FILTER_REPLACENEWFILES |
            RPMPROB_FILTER_OLDPACKAGE;

    filterflags |= RPMPROB_FILTER_DISKSPACE;
    instflags |= INSTALL_NOORDER | INSTALL_UPGRADE;

    return rpm_install(db->dbh, db->rootdir, path,
                       filterflags, transflags, instflags);
}

int pkgdb_map(struct pkgdb *db,
              void (*mapfn)(unsigned recno, void *header, void *arg),
              void *arg) 
{
    return rpm_dbmap(db->dbh, mapfn, arg);
}

