/*
 * Copyright 2020-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/crypto.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "crypto/ecx.h"
#include "prov/implementations.h"
#include "prov/providercommon.h"

static OSSL_FUNC_keyexch_newctx_fn x25519_newctx;
static OSSL_FUNC_keyexch_newctx_fn x448_newctx;
static OSSL_FUNC_keyexch_init_fn ecx_init;
static OSSL_FUNC_keyexch_set_peer_fn ecx_set_peer;
static OSSL_FUNC_keyexch_derive_fn ecx_derive;
static OSSL_FUNC_keyexch_freectx_fn ecx_freectx;
static OSSL_FUNC_keyexch_dupctx_fn ecx_dupctx;

/*
 * What's passed as an actual key is defined by the KEYMGMT interface.
 * We happen to know that our KEYMGMT simply passes ECX_KEY structures, so
 * we use that here too.
 */

typedef struct {
    size_t keylen;
    ECX_KEY *key;
    ECX_KEY *peerkey;
} PROV_ECX_CTX;

static void *ecx_newctx(void *provctx, size_t keylen)
{
    PROV_ECX_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = zalloc(PROV_ECX_CTX, 1);
    if (ctx == NULL)
        return NULL;

    ctx->keylen = keylen;

    return ctx;
}

static void *x25519_newctx(void *provctx)
{
    return ecx_newctx(provctx, X25519_KEYLEN);
}

static void *x448_newctx(void *provctx)
{
    return ecx_newctx(provctx, X448_KEYLEN);
}

static int ecx_init(void *vecxctx, void *vkey,
                    ossl_unused const OSSL_PARAM params[])
{
    PROV_ECX_CTX *ecxctx = (PROV_ECX_CTX *)vecxctx;
    ECX_KEY *key = vkey;

    if (!ossl_prov_is_running())
        return 0;

    if (ecxctx == NULL
            || key == NULL
            || key->keylen != ecxctx->keylen
            || !ossl_ecx_key_up_ref(key)) {
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    ossl_ecx_key_free(ecxctx->key);
    ecxctx->key = key;

    return 1;
}

static int ecx_set_peer(void *vecxctx, void *vkey)
{
    PROV_ECX_CTX *ecxctx = (PROV_ECX_CTX *)vecxctx;
    ECX_KEY *key = vkey;

    if (!ossl_prov_is_running())
        return 0;

    if (ecxctx == NULL
            || key == NULL
            || key->keylen != ecxctx->keylen
            || !ossl_ecx_key_up_ref(key)) {
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        return 0;
    }
    ossl_ecx_key_free(ecxctx->peerkey);
    ecxctx->peerkey = key;

    return 1;
}

static int ecx_derive(void *vecxctx, unsigned char *secret, size_t *secretlen,
                      size_t outlen)
{
    PROV_ECX_CTX *ecxctx = (PROV_ECX_CTX *)vecxctx;

    if (!ossl_prov_is_running())
        return 0;
    return ossl_ecx_compute_key(ecxctx->peerkey, ecxctx->key, ecxctx->keylen,
                                secret, secretlen, outlen);
}

static void ecx_freectx(void *vecxctx)
{
    PROV_ECX_CTX *ecxctx = (PROV_ECX_CTX *)vecxctx;

    ossl_ecx_key_free(ecxctx->key);
    ossl_ecx_key_free(ecxctx->peerkey);

    OPENSSL_free(ecxctx);
}

static void *ecx_dupctx(void *vecxctx)
{
    PROV_ECX_CTX *srcctx = (PROV_ECX_CTX *)vecxctx;
    PROV_ECX_CTX *dstctx;

    if (!ossl_prov_is_running())
        return NULL;

    dstctx = zalloc(typeof(*srcctx), 1);
    if (dstctx == NULL)
        return NULL;

    *dstctx = *srcctx;
    if (dstctx->key != NULL && !ossl_ecx_key_up_ref(dstctx->key)) {
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        OPENSSL_free(dstctx);
        return NULL;
    }

    if (dstctx->peerkey != NULL && !ossl_ecx_key_up_ref(dstctx->peerkey)) {
        ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
        ossl_ecx_key_free(dstctx->key);
        OPENSSL_free(dstctx);
        return NULL;
    }

    return dstctx;
}

const OSSL_DISPATCH ossl_x25519_keyexch_functions[] = {
    { OSSL_FUNC_KEYEXCH_NEWCTX, (void (*)(void))x25519_newctx },
    { OSSL_FUNC_KEYEXCH_INIT, (void (*)(void))ecx_init },
    { OSSL_FUNC_KEYEXCH_DERIVE, (void (*)(void))ecx_derive },
    { OSSL_FUNC_KEYEXCH_SET_PEER, (void (*)(void))ecx_set_peer },
    { OSSL_FUNC_KEYEXCH_FREECTX, (void (*)(void))ecx_freectx },
    { OSSL_FUNC_KEYEXCH_DUPCTX, (void (*)(void))ecx_dupctx },
    OSSL_DISPATCH_END
};

const OSSL_DISPATCH ossl_x448_keyexch_functions[] = {
    { OSSL_FUNC_KEYEXCH_NEWCTX, (void (*)(void))x448_newctx },
    { OSSL_FUNC_KEYEXCH_INIT, (void (*)(void))ecx_init },
    { OSSL_FUNC_KEYEXCH_DERIVE, (void (*)(void))ecx_derive },
    { OSSL_FUNC_KEYEXCH_SET_PEER, (void (*)(void))ecx_set_peer },
    { OSSL_FUNC_KEYEXCH_FREECTX, (void (*)(void))ecx_freectx },
    { OSSL_FUNC_KEYEXCH_DUPCTX, (void (*)(void))ecx_dupctx },
    OSSL_DISPATCH_END
};
