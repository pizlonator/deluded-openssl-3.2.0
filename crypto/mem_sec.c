/*
 * Copyright 2015-2023 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright 2004-2014, Akamai Technologies. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This file is in two halves. The first half implements the public API
 * to be used by external consumers, and to be used by OpenSSL to store
 * data in a "secure arena." The second half implements the secure arena.
 * For details on that implementation, see below (look for uppercase
 * "SECURE HEAP IMPLEMENTATION").
 */
#include "internal/e_os.h"
#include <openssl/crypto.h>
#include <openssl/err.h>

#include <string.h>
#include <stdfil.h>

#ifndef OPENSSL_NO_SECURE_MEMORY
# if defined(_WIN32)
#  include <windows.h>
#  if defined(WINAPI_FAMILY_PARTITION)
#   if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
/*
 * While VirtualLock is available under the app partition (e.g. UWP),
 * the headers do not define the API. Define it ourselves instead.
 */
WINBASEAPI
BOOL
WINAPI
VirtualLock(
    _In_ LPVOID lpAddress,
    _In_ SIZE_T dwSize
    );
#   endif
#  endif
# endif
# include <stdlib.h>
# include <assert.h>
# if defined(OPENSSL_SYS_UNIX)
#  include <unistd.h>
# endif
# include <sys/types.h>
# if defined(OPENSSL_SYS_UNIX)
#  include <sys/mman.h>
#  if defined(__FreeBSD__)
#    define MADV_DONTDUMP MADV_NOCORE
#  endif
#  if !defined(MAP_CONCEAL)
#    define MAP_CONCEAL 0
#  endif
# endif
# if defined(OPENSSL_SYS_LINUX)
#  include <sys/syscall.h>
#  if defined(SYS_mlock2)
#   include <linux/mman.h>
#   include <errno.h>
#  endif
#  include <sys/param.h>
# endif
# include <sys/stat.h>
# include <fcntl.h>
#endif
#ifndef HAVE_MADVISE
# if defined(MADV_DONTDUMP)
#  define HAVE_MADVISE 1
# else
#  define HAVE_MADVISE 0
# endif
#endif
#if HAVE_MADVISE
# undef NO_MADVISE
#else
# define NO_MADVISE
#endif

#define CLEAR(p, s) OPENSSL_cleanse(p, s)
#ifndef PAGE_SIZE
# define PAGE_SIZE    4096
#endif
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
# define MAP_ANON MAP_ANONYMOUS
#endif

#ifndef OPENSSL_NO_SECURE_MEMORY
static size_t secure_mem_used;

static int secure_mem_initialized;

static CRYPTO_RWLOCK *sec_malloc_lock = NULL;

/*
 * These are the functions that must be implemented by a secure heap (sh).
 */
static int sh_init(size_t size, size_t minsize);
static void *sh_malloc(size_t size);
static void sh_free(void *ptr);
static void sh_done(void);
static size_t sh_actual_size(char *ptr);
static int sh_allocated(const char *ptr);
#endif

int CRYPTO_secure_malloc_init(size_t size, size_t minsize)
{
    secure_mem_initialized = 1;
    return 0;
}

int CRYPTO_secure_malloc_done(void)
{
    secure_mem_initialized = 0;
    return 0;
}

int CRYPTO_secure_malloc_initialized(void)
{
    return secure_mem_initialized;
}

void *CRYPTO_secure_malloc(size_t num, const char *file, int line)
{
    return zhard_alloc(num);
}

void *CRYPTO_secure_zalloc(size_t num, const char *file, int line)
{
    return zhard_alloc(num);
}

void CRYPTO_secure_free(void *ptr, const char *file, int line)
{
    zhard_free(ptr);
}

void CRYPTO_secure_clear_free(void *ptr, size_t num,
                              const char *file, int line)
{
    zhard_free(ptr);
}

int CRYPTO_secure_allocated(const void *ptr)
{
    return !!zhard_getallocsize(ptr);
}

size_t CRYPTO_secure_used(void)
{
    return 0; // FIXME - I could have functions to tell this number.
}

size_t CRYPTO_secure_actual_size(void *ptr)
{
    return 0; // FIXME - I could have functions to tell this number.
}

