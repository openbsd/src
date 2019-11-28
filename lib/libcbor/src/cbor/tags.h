/*
 * Copyright (c) 2014-2019 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef LIBCBOR_TAGS_H
#define LIBCBOR_TAGS_H

#include "cbor/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ============================================================================
 * Tag manipulation
 * ============================================================================
 */

/** Create a new tag
 *
 * @param value The tag value. Please consult the tag repository
 * @return **new** tag. Item reference is `NULL`. Returns `NULL` upon
 * 	memory allocation failure
 */
cbor_item_t *cbor_new_tag(uint64_t value);

/** Get the tagged item
 *
 * @param item[borrow] A tag
 * @return **incref** the tagged item
 */
cbor_item_t *cbor_tag_item(const cbor_item_t *item);

/** Get tag value
 *
 * @param item[borrow] A tag
 * @return The tag value. Please consult the tag repository
 */
uint64_t cbor_tag_value(const cbor_item_t *item);

/** Set the tagged item
 *
 * @param item[borrow] A tag
 * @param tagged_item[incref] The item to tag
 */
void cbor_tag_set_item(cbor_item_t *item, cbor_item_t *tagged_item);

/** Build a new tag
 *
 * @param item[incref] The tagee
 * @param value Tag value
 * @return **new** tag item
 */
cbor_item_t *cbor_build_tag(uint64_t value, cbor_item_t *item);

#ifdef __cplusplus
}
#endif

#endif  // LIBCBOR_TAGS_H
