#!/bin/sh
# $Id$

if [ ! -f capreq.h ]; then
    cd `dirname $0`;
    if [ ! -f capreq.h ]; then
	echo "`pwd`: not a poldek directory"
	exit 1
    fi 	
fi

runcmd () {
    echo "$@"
    $@
}

runcmd aclocal
runcmd autoheader
runcmd autoconf
runcmd automake --add-missing 

# w/o
if [ ! -f depcomp ]; then 
	runcmd automake --add-missing Makefile
	(cd vfile && ln -sf ../depcomp .)
        (cd shell && ln -sf ../depcomp .)
fi

CONFOPTS="--enable-maintainer-mode --enable-compile-warnings $@"
runcmd ./configure $CONFOPTS
