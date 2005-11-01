/* $Id$ */
/* ini-like config parsing module */


#ifndef POLDEK_CONF_INTERN_H
#define POLDEK_CONF_INTERN_H

#define CONF_TYPE_STRING        (1 << 0)
#define CONF_TYPE_BOOLEAN       (1 << 1)
#define CONF_TYPE_BOOLEAN3      (1 << 2) /* yes, no, auto */
#define CONF_TYPE_INTEGER       (1 << 3)
#define CONF_TYPE_ENUM          (1 << 4)

#define CONF_TYPE_F_LIST       (1 << 5) /* values list */
#define CONF_TYPE_F_PATH       (1 << 6) /* path -"- */
#define CONF_TYPE_F_MULTI      (1 << 7) /* may occurr multiple times */
#define CONF_TYPE_F_MULTI_EXCL (1 << 8) /* may occurr multiple times, last occurrence
                                      is taken into account */

#define CONF_TYPE_F_ENV        (1 << 10) /* environment variables are expanded */
#define CONF_TYPE_F_REQUIRED   (1 << 11) /*  */
#define CONF_TYPE_F_ALIAS      (1 << 12) /* an alias */
#define CONF_TYPE_F_OBSL       (1 << 14) /* obsoleted */

struct poldek_conf_tag {
    char      *name;
    unsigned  flags;
    char      *defaultv;
    int       _op_no;           /* internal option number */
    char      *enums[8];
};

struct poldek_conf_section {
    char                    *name;
    struct poldek_conf_tag  *tags;
    int                     is_multi;
};

extern struct poldek_conf_section poldek_conf_sections[];

#endif
