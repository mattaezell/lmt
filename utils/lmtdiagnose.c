/*****************************************************************************
 *  Copyright (C) 2010 Lawrence Livermore National Security, LLC.
 *  This module written by Jim Garlick <garlick@llnl.gov>
 *  UCRL-CODE-232438
 *  All Rights Reserved.
 *
 *  This file is part of Lustre Monitoring Tool, version 2.
 *  Authors: H. Wartens, P. Spencer, N. O'Neill, J. Long, J. Garlick
 *  For details, see http://code.google.com/p/lmt/.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License (as published by the
 *  Free Software Foundation) version 2, dated June 1991.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA or see
 *  <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#if HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <libgen.h>

#include "list.h"
#include "hash.h"

#include "proc.h"
#include "lmt.h"

#include "ost.h"
#include "mdt.h"
#include "router.h"
#include "lmtmysql.h"
#include "lmtcerebro.h"

#define OPTIONS "f:"
#if HAVE_GETOPT_LONG
#define GETOPT(ac,av,opt,lopt) getopt_long (ac,av,opt,lopt,NULL)
static const struct option longopts[] = {
    {"filesystem",      required_argument,  0, 'f'},
    {0, 0, 0, 0},
};
#else
#define GETOPT(ac,av,opt,lopt) getopt (ac,av,opt)
#endif

#define CURRENT_METRIC_NAMES    "lmt_mdt,lmt_ost,lmt_router"
#define LEGACY_METRIC_NAMES     "lmt_oss,lmt_mds"
#define METRIC_NAMES            CURRENT_METRIC_NAMES","LEGACY_METRIC_NAMES

static char *prog;

/* default to localhost:3306 root:"" */
static const char *db_host = "localhost";
static const unsigned int db_port = 0;
static const char *db_user = "lwatchclient";
static const char *db_passwd = NULL;

void check_cerebro (void);
void check_mysql (void);


static void
usage()
{
    fprintf (stderr,
"Usage: lmtdiagnose [OPTIONS]\n"
"   -f,--filesystem NAME        select file system [default all]\n"
    );
    exit (1);
}

int
main (int argc, char *argv[])
{
    char *fs = NULL;
    int c;

    prog = basename (argv[0]);
    optind = 0;
    opterr = 0;
    while ((c = GETOPT (argc, argv, OPTIONS, longopts)) != -1) {
        switch (c) {
            case 'f':   /* --filesystem NAME */
                fs = optarg;
                break;
            default:
                usage ();
        }
    }
    if (optind < argc)
        usage();

    check_cerebro ();
    check_mysql ();

    exit (0);
}

int
_parse_ost_v2 (char *s, const char **errp)
{
    int retval = -1;
    return retval;
}

int
_parse_mdt_v1 (char *s, const char **errp)
{
    int retval = -1;
    return retval;
}

int
_parse_router_v1 (char *s, const char **errp)
{
    int retval = -1;
    char *name = NULL;
    float pct_cpu, pct_mem;
    uint64_t bytes;

    if (lmt_router_decode_v1 (s, &name, &pct_cpu, &pct_mem, &bytes) < 0)
        goto done;
    retval = 0;
done:
    if (name)
        free (name);
    return retval;
}

int
_parse_mds_v2 (char *s, const char **errp)
{
    int retval = -1;
    char *mdsname = NULL;
    char *name = NULL;
    float pct_cpu, pct_mem;
    uint64_t inodes_free, inodes_total;
    uint64_t kbytes_free, kbytes_total;
    List mdops = NULL;
    ListIterator itr = NULL;
    char *opname, *op;
    uint64_t samples, sum, sumsq;

    if (lmt_mds_decode_v2 (s, &mdsname, &name, &pct_cpu, &pct_mem,
          &inodes_free, &inodes_total, &kbytes_free, &kbytes_total, &mdops) < 0)
        goto done;
    if (!(itr = list_iterator_create (mdops)))
        goto done;
    while ((op = list_next (itr))) {
        if (lmt_mds_decode_v2_mdops (op, &opname, &samples, &sum, &sumsq) < 0)
            goto done;
        free (opname);
    }
    retval = 0;
done:
    if (itr)
        list_iterator_destroy (itr);
    if (name)
        free (name); 
    if (mdsname)
        free (mdsname); 
    if (mdops)
        list_destroy (mdops);
    return retval;
}

int
_parse_oss_v1 (char *s, const char **errp)
{
    int retval = -1;
    char *name = NULL;
    float pct_cpu, pct_mem;

    if (lmt_oss_decode_v1 (s, &name, &pct_cpu, &pct_mem) < 0)
        goto done;
    retval = 0;
done:
    if (name)
        free (name);
    return retval;
}

int
_parse_ost_v1 (char *s, const char **errp)
{
    int retval = -1;
    char *ossname = NULL;
    char *name = NULL;
    uint64_t read_bytes, write_bytes;
    uint64_t kbytes_free, kbytes_total;
    uint64_t inodes_free, inodes_total;

    if (lmt_ost_decode_v1 (s, &ossname, &name, &read_bytes, &write_bytes,
              &kbytes_free, &kbytes_total, &inodes_free, &inodes_total) < 0)
        goto done;
    retval = 0;
done:
    if (name)
        free (name);
    if (ossname)
        free (ossname);
    return retval;
}

void
check_cerebro (void)
{
    List l = NULL;;
    ListIterator itr;
    char *name, *val;
    cmetric_t c;
    float vers;
    const char *errstr = NULL;
    int err;

    if (lmt_cbr_get_metrics (METRIC_NAMES, &l, (char **)&errstr) < 0) {
        fprintf (stderr, "%s: lmt_ost: %s\n", prog,
                 errstr ? errstr : strerror (errno));
        exit (1);
    }
    if (!(itr = list_iterator_create (l))) {
        fprintf (stderr, "%s: out of memory\n", prog);
        exit (1);
    }
    while ((c = list_next (itr))) {
        name = lmt_cbr_get_name (c);
        val = lmt_cbr_get_val (c);
        if (!val)
            continue;
        if (sscanf (val, "%f;", &vers) != 1) {
            fprintf (stderr, "%s: error parsing metric version\n", prog);
            continue; 
        }
        errstr = NULL;
        if (!strcmp (name, "lmt_ost") && vers == 2)
            err = _parse_ost_v2 (val, &errstr);
        else if (!strcmp (name, "lmt_mdt") && vers == 1)
            err = _parse_mdt_v1 (val, &errstr);
        else if (!strcmp (name, "lmt_router") && vers == 1)
            err = _parse_router_v1 (val, &errstr);
        else if (!strcmp (name, "lmt_mds") && vers == 2)
            err = _parse_mds_v2 (val, &errstr);
        else if (!strcmp (name, "lmt_oss") && vers == 1)
            err = _parse_oss_v1 (val, &errstr);
        else if (!strcmp (name, "lmt_ost") && vers == 1)
            err = _parse_ost_v1 (val, &errstr);
        else {
            fprintf (stderr, "%s: %s_v%d: unknown metric version\n", prog,
                     name, (int)vers);
            continue;
        }
        if (err < 0) {
            fprintf (stderr, "%s: %s_v%d: %s\n", prog,
                     name, (int)vers, errstr ? errstr : strerror (errno));
        }
    }
    list_iterator_destroy (itr);
    list_destroy (l);
}

void
check_mysql (void)
{
    List dbs = NULL;
    const char *errstr = NULL;
    ListIterator itr;
    lmt_db_t db;

    if (lmt_db_create_all (db_host, db_port, db_user, db_passwd,
                                                    &dbs, &errstr) < 0) {
        fprintf (stderr, "%s: %s\n", prog, errstr ? errstr : strerror (errno));
        exit (1);
    }
    if (list_is_empty (dbs)) {
        fprintf (stderr, "%s: mysql has no file systems configured\n", prog);
        exit (1);
    }
    if (!(itr = list_iterator_create (dbs))) {
        fprintf (stderr, "%s: out of memory\n", prog);
        exit (1);
    }
    while ((db = list_next (itr))) {
        fprintf (stderr, "%s: mysql: %s\n", prog, lmt_db_name (db));
    }
    list_iterator_destroy (itr);

    list_destroy (dbs);
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

