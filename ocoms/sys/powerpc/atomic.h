/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2010      IBM Corporation.  All rights reserved.
 * Copyright (c) 2011-2013 UT-Battelle, LLC. All rights reserved.
 * Copyright (C) 2013      Mellanox Technologies Ltd. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef OCOMS_SYS_ARCH_ATOMIC_H
#define OCOMS_SYS_ARCH_ATOMIC_H 1

/*
 * On powerpc ...
 */

#if OCOMS_WANT_SMP_LOCKS

#define MB()  __asm__ __volatile__ ("sync" : : : "memory")
#define RMB() __asm__ __volatile__ ("lwsync" : : : "memory")
#define WMB() __asm__ __volatile__ ("eieio" : : : "memory")
#define ISYNC() __asm__ __volatile__ ("isync" : : : "memory")
#define SMP_SYNC  "sync \n\t"
#define SMP_ISYNC "\n\tisync"

#else

#define MB()
#define RMB()
#define WMB()
#define SMP_SYNC  ""
#define SMP_ISYNC

#endif


/**********************************************************************
 *
 * Define constants for PowerPC 32
 *
 *********************************************************************/
#define OCOMS_HAVE_ATOMIC_MEM_BARRIER 1

#define OCOMS_HAVE_ATOMIC_CMPSET_32 1

#define OCOMS_HAVE_ATOMIC_MATH_32 1
#define OCOMS_HAVE_ATOMIC_ADD_32 1
#define OCOMS_HAVE_ATOMIC_SUB_32 1


#if (OCOMS_ASSEMBLY_ARCH == OCOMS_POWERPC64) || OCOMS_ASM_SUPPORT_64BIT
#define OCOMS_HAVE_ATOMIC_CMPSET_64 1
#endif


/**********************************************************************
 *
 * Memory Barriers
 *
 *********************************************************************/
#if OCOMS_GCC_INLINE_ASSEMBLY

static inline
void ocoms_atomic_mb(void)
{
    MB();
}


static inline
void ocoms_atomic_rmb(void)
{
    RMB();
}


static inline
void ocoms_atomic_wmb(void)
{
    RMB();
}

static inline
void ocoms_atomic_isync()
{
    ISYNC();
}

#elif OCOMS_XLC_INLINE_ASSEMBLY /* end OCOMS_GCC_INLINE_ASSEMBLY */

/* Yeah, I don't know who thought this was a reasonable syntax for
 * inline assembly.  Do these because they are used so often and they
 * are fairly simple (aka: there is a tech pub on IBM's web site
 * containing the right hex for the instructions).
 */

#undef OCOMS_HAVE_INLINE_ATOMIC_MEM_BARRIER
#define OCOMS_HAVE_INLINE_ATOMIC_MEM_BARRIER 0

#pragma mc_func ocoms_atomic_mb { "7c0004ac" }          /* sync  */
#pragma reg_killed_by ocoms_atomic_mb                   /* none */

#pragma mc_func ocoms_atomic_rmb { "7c2004ac" }         /* lwsync  */
#pragma reg_killed_by ocoms_atomic_rmb                  /* none */

#pragma mc_func ocoms_atomic_wmb { "7c0006ac" }         /* eieio */
#pragma reg_killed_by ocoms_atomic_wmb                  /* none */

#endif

/**********************************************************************
 *
 * Atomic math operations
 *
 *********************************************************************/
#if OCOMS_GCC_INLINE_ASSEMBLY

static inline int ocoms_atomic_cmpset_32(volatile int32_t *addr,
                                        int32_t oldval, int32_t newval)
{
   int32_t ret;

   __asm__ __volatile__ (
                         "1: lwarx   %0, 0, %2  \n\t"
                         "   cmpw    0, %0, %3  \n\t"
                         "   bne-    2f         \n\t"
                         "   stwcx.  %4, 0, %2  \n\t"
                         "   bne-    1b         \n\t"
                         "2:"
                         : "=&r" (ret), "=m" (*addr)
                         : "r" (addr), "r" (oldval), "r" (newval), "m" (*addr)
                         : "cc", "memory");

   return (ret == oldval);
}

/* these two functions aren't inlined in the non-gcc case because then
   there would be two function calls (since neither cmpset_32 nor
   atomic_?mb can be inlined).  Instead, we "inline" them by hand in
   the assembly, meaning there is one function call overhead instead
   of two */
static inline int ocoms_atomic_cmpset_acq_32(volatile int32_t *addr,
                                            int32_t oldval, int32_t newval)
{
    int rc;

    rc = ocoms_atomic_cmpset_32(addr, oldval, newval);
    ocoms_atomic_rmb();

    return rc;
}


static inline int ocoms_atomic_cmpset_rel_32(volatile int32_t *addr,
                                            int32_t oldval, int32_t newval)
{
    ocoms_atomic_wmb();
    return ocoms_atomic_cmpset_32(addr, oldval, newval);
}

#endif /* OCOMS_GCC_INLINE_ASSEMBLY */


#if (OCOMS_ASSEMBLY_ARCH == OCOMS_POWERPC64)

#if  OCOMS_GCC_INLINE_ASSEMBLY
static inline int ocoms_atomic_cmpset_64(volatile int64_t *addr,
                                        int64_t oldval, int64_t newval)
{
   int64_t ret;

   __asm__ __volatile__ (
                         "1: ldarx   %0, 0, %2  \n\t"
                         "   cmpd    0, %0, %3  \n\t"
                         "   bne-    2f         \n\t"
                         "   stdcx.  %4, 0, %2  \n\t"
                         "   bne-    1b         \n\t"
                         "2:"
                         : "=&r" (ret), "=m" (*addr)
                         : "r" (addr), "r" (oldval), "r" (newval), "m" (*addr)
                         : "cc", "memory");
    
   return (ret == oldval);
}

/* these two functions aren't inlined in the non-gcc case because then
   there would be two function calls (since neither cmpset_64 nor
   atomic_?mb can be inlined).  Instead, we "inline" them by hand in
   the assembly, meaning there is one function call overhead instead
   of two */
static inline int ocoms_atomic_cmpset_acq_64(volatile int64_t *addr,
                                            int64_t oldval, int64_t newval)
{
    int rc;

    rc = ocoms_atomic_cmpset_64(addr, oldval, newval);
    ocoms_atomic_rmb();

    return rc;
}


static inline int ocoms_atomic_cmpset_rel_64(volatile int64_t *addr,
                                            int64_t oldval, int64_t newval)
{
    ocoms_atomic_wmb();
    return ocoms_atomic_cmpset_64(addr, oldval, newval);
}

#endif /* OCOMS_GCC_INLINE_ASSEMBLY */

#elif (OCOMS_ASSEMBLY_ARCH == OCOMS_POWERPC32) && OCOMS_ASM_SUPPORT_64BIT

#ifndef ll_low /* GLIBC provides these somewhere, so protect */
#define ll_low(x)       *(((unsigned int*)&(x))+0)
#define ll_high(x)      *(((unsigned int*)&(x))+1)
#endif

#if  OCOMS_GCC_INLINE_ASSEMBLY

static inline int ocoms_atomic_cmpset_64(volatile int64_t *addr,
                                        int64_t oldval, int64_t newval)
{
    int ret;

    /*
     * We force oldval and newval into memory because PPC doesn't
     * appear to have a way to do a move register with offset.  Since
     * this is 32-bit code, a 64 bit integer will be loaded into two
     * registers (assuming no inlining, addr will be in r3, oldval
     * will be in r4 and r5, and newval will be r6 and r7.  We need
     * to load the whole thing into one register.  So we have the
     * compiler push the values into memory and load the double word
     * into registers.  We use r4,r5 so that the main block of code
     * is very similar to the pure 64 bit version.
     */
   __asm__ __volatile__ (
                         "ld r4,%2         \n\t"
                         "ld r5,%3        \n\t"
                         "1: ldarx   r9, 0, %1  \n\t"
                         "   cmpd    0, r9, r4  \n\t"
                         "   bne-    2f         \n\t"
                         "   stdcx.  r5, 0, %1  \n\t"
                         "   bne-    1b         \n\t"
                         "2:                    \n\t"
                         "xor r5,r4,r9          \n\t"
                         "subfic r9,r5,0        \n\t"
                         "adde %0,r9,r5         \n\t"
                         : "=&r" (ret)
                         : "r"(addr), 
                           "m"(oldval), "m"(newval)
                         : "r4", "r5", "r9", "cc", "memory");
    
     return ret;
}

/* these two functions aren't inlined in the non-gcc case because then
   there would be two function calls (since neither cmpset_64 nor
   atomic_?mb can be inlined).  Instead, we "inline" them by hand in
   the assembly, meaning there is one function call overhead instead
   of two */
static inline int ocoms_atomic_cmpset_acq_64(volatile int64_t *addr,
                                            int64_t oldval, int64_t newval)
{
    int rc;

    rc = ocoms_atomic_cmpset_64(addr, oldval, newval);
    ocoms_atomic_rmb();

    return rc;
}


static inline int ocoms_atomic_cmpset_rel_64(volatile int64_t *addr,
                                            int64_t oldval, int64_t newval)
{
    ocoms_atomic_wmb();
    return ocoms_atomic_cmpset_64(addr, oldval, newval);
}

#endif /* OCOMS_GCC_INLINE_ASSEMBLY */

#endif /* OCOMS_ASM_SUPPORT_64BIT */


#if OCOMS_GCC_INLINE_ASSEMBLY

static inline int32_t ocoms_atomic_add_32(volatile int32_t* v, int inc)
{
   int32_t t;

   __asm__ __volatile__(
                        "1:   lwarx   %0, 0, %3    \n\t"
                        "     add     %0, %2, %0   \n\t"
                        "     stwcx.  %0, 0, %3    \n\t"
                        "     bne-    1b           \n\t"
                        : "=&r" (t), "=m" (*v)
                        : "r" (inc), "r" (v), "m" (*v)
                        : "cc");

   return t;
}


static inline int32_t ocoms_atomic_sub_32(volatile int32_t* v, int dec)
{
   int32_t t;

   __asm__ __volatile__(
                        "1:   lwarx   %0,0,%3      \n\t"
                        "     subf    %0,%2,%0     \n\t"
                        "     stwcx.  %0,0,%3      \n\t"
                        "     bne-    1b           \n\t"
                        : "=&r" (t), "=m" (*v)
                        : "r" (dec), "r" (v), "m" (*v)
                        : "cc");

   return t;
}


#endif /* OCOMS_GCC_INLINE_ASSEMBLY */

#endif /* ! OCOMS_SYS_ARCH_ATOMIC_H */
