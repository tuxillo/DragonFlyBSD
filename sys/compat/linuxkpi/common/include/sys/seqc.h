/*-
 * Minimal seqc stub for LinuxKPI on DragonFly.
 */

#ifndef _SYS_SEQC_H_
#define _SYS_SEQC_H_

#include <sys/types.h>
#include <sys/cdefs.h>
#include <machine/atomic.h>

typedef u_int seqc_t;

#define SEQC_MOD 1

static __inline void
seqc_sleepable_write_begin(seqc_t *seqcp)
{
	atomic_add_int(seqcp, SEQC_MOD);
}

static __inline void
seqc_sleepable_write_end(seqc_t *seqcp)
{
	atomic_add_int(seqcp, SEQC_MOD);
}

static __inline void
seqc_write_begin(seqc_t *seqcp)
{
	atomic_add_int(seqcp, SEQC_MOD);
}

static __inline void
seqc_write_end(seqc_t *seqcp)
{
	atomic_add_int(seqcp, SEQC_MOD);
}

static __inline seqc_t
seqc_read(const seqc_t *seqcp)
{
	return atomic_load_int(__DECONST(seqc_t *, seqcp));
}

static __inline seqc_t
seqc_read_any(const seqc_t *seqcp)
{
	return atomic_load_int(__DECONST(seqc_t *, seqcp));
}

static __inline int
seqc_consistent_no_fence(const seqc_t *seqcp, seqc_t start)
{
	seqc_t end = atomic_load_int(__DECONST(seqc_t *, seqcp));
	return ((start == end) && ((start & 1) == 0));
}

static __inline int
seqc_consistent(const seqc_t *seqcp, seqc_t start)
{
	return seqc_consistent_no_fence(seqcp, start);
}

#endif
