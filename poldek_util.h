/*
  Copyright (C) 2000 - 2008 Pawel A. Gajda <mis@pld-linux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2 as
  published by the Free Software Foundation (see file COPYING for details).

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef POLDEK_UTIL_H

#include <trurl/nhash.h>
#include <time.h>

#ifndef EXPORT
#  define EXPORT extern
#endif

EXPORT const char *poldek_util_lc_lang(const char *category);
EXPORT int poldek_util_get_gmt_offs(void);
EXPORT int poldek_util_is_rwxdir(const char *path);
EXPORT time_t poldek_util_mtime(const char *path);

EXPORT const char *poldek_util_ngettext_n_packages_fmt(int n);

EXPORT int poldek_util_parse_bool(const char *v);

/* returns 0 - false, 1 - true, 2 - auto */
EXPORT int poldek_util_parse_bool3(const char *v);


/* remove used variables from varh */
#define POLDEK_UTIL_EXPANDVARS_RMUSED (1 << 0)

/* expands "foo %{foo} bar */
EXPORT const char *poldek_util_expand_vars(char *dest, int size, const char *src,
                                    char varmark, tn_hash *varh,
                                    unsigned flags);

EXPORT const char *poldek_util_expand_env_vars(char *dest, int size, const char *str);

EXPORT int poldek_util_copy_file(const char *src, const char *dst);

#endif
