/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/make/lst.lib/lstForEachFrom.c,v 1.7 1999/08/28 01:03:52 peter Exp $
 * $DragonFly: src/usr.bin/make/lst.lib/Attic/lstForEachFrom.c,v 1.7 2004/12/09 19:08:36 okumoto Exp $
 *
 * @(#)lstForEachFrom.c	8.1 (Berkeley) 6/6/93
 */

/*-
 * lstForEachFrom.c --
 *	Perform a given function on all elements of a list starting from
 *	a given point.
 */

#include "make.h"
#include "lst.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_ForEachFrom --
 *	Apply the given function to each element of the given list. The
 *	function should return 0 if traversal should continue and non-
 *	zero if it should abort.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Only those created by the passed-in function.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_ForEachFrom(Lst list, LstNode ln, DoProc *proc, void *d)
{
    LstNode	next;
    Boolean 	done;
    int     	result;

    if (!Lst_Valid (list) || Lst_IsEmpty (list)) {
	return;
    }

    do {
	/*
	 * Take care of having the current element deleted out from under
	 * us.
	 */

	next = ln->nextPtr;

	(void) ln->useCount++;
	result = (*proc) (ln->datum, d);
	(void) ln->useCount--;

	/*
	 * We're done with the traversal if
	 *  - nothing's been added after the current node and
	 *  - the next node to examine is the first in the queue or
	 *    doesn't exist.
	 */
	done = (next == ln->nextPtr &&
		(next == NULL || next == list->firstPtr));

	next = ln->nextPtr;

	if (ln->flags & LN_DELETED) {
	    free(ln);
	}
	ln = next;
    } while (!result && !Lst_IsEmpty(list) && !done);
}
