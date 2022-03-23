/*
 * Copyright (c) 2018-2021 mcumgr authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/util.h>
#include <string.h>
#include <stdio.h>

#include <stats/stats.h>
#include <mgmt/mcumgr/buf.h>
#include <mgmt/mgmt.h>
#include <stat_mgmt/stat_mgmt_config.h>
#include <stat_mgmt/stat_mgmt.h>
#include "cborattr/cborattr.h"
#include <zcbor_common.h>
#include <zcbor_encode.h>

static struct mgmt_handler stat_mgmt_handlers[];

typedef int stat_mgmt_foreach_entry_fn(zcbor_state_t *zse, struct stat_mgmt_entry *entry);

static int
stats_mgmt_count_plus_one(struct stats_hdr *hdr, void *arg, const char *name, uint16_t off)
{
	size_t *counter = arg;

	(*counter)++;

	return 0;
}

static int
stat_mgmt_count(const char *group_name, size_t *counter)
{
	struct stats_hdr *hdr = stats_group_find(group_name);

	if (hdr == NULL) {
		return MGMT_ERR_ENOENT;
	}

	*counter = 0;

	return stats_walk(hdr, stats_mgmt_count_plus_one, &counter);
}

static int
stat_mgmt_walk_cb(struct stats_hdr *hdr, void *arg, const char *name, uint16_t off);

struct stat_mgmt_walk_arg {
	stat_mgmt_foreach_entry_fn *cb;
	zcbor_state_t *zse;
};

static int
stat_mgmt_walk_cb(struct stats_hdr *hdr, void *arg, const char *name, uint16_t off)
{
	struct stat_mgmt_walk_arg *walk_arg;
	struct stat_mgmt_entry entry;
	void *stat_val;

	walk_arg = arg;

	stat_val = (uint8_t *)hdr + off;
	switch (hdr->s_size) {
	case sizeof(uint16_t):
		entry.value = *(uint16_t *) stat_val;
		break;
	case sizeof(uint32_t):
		entry.value = *(uint32_t *) stat_val;
		break;
	case sizeof(uint64_t):
		entry.value = *(uint64_t *) stat_val;
		break;
	default:
		return MGMT_ERR_EINVAL;
	}
	entry.name = name;

	return walk_arg->cb(walk_arg->zse, &entry);
}

static int
stat_mgmt_foreach_entry(zcbor_state_t *zse, const char *group_name, stat_mgmt_foreach_entry_fn *cb)
{
	struct stat_mgmt_walk_arg walk_arg;
	struct stats_hdr *hdr;

	hdr = stats_group_find(group_name);
	if (hdr == NULL) {
		return MGMT_ERR_ENOENT;
	}

	walk_arg = (struct stat_mgmt_walk_arg) {
		.cb = cb,
		.zse = zse
	};

	return stats_walk(hdr, stat_mgmt_walk_cb, &walk_arg);
}

static int
stat_mgmt_cb_encode(zcbor_state_t *zse, struct stat_mgmt_entry *entry)
{
	bool ok = zcbor_tstr_put_term(zse, entry->name) &&
		  zcbor_uint32_put(zse, entry->value);

	return ok ? MGMT_ERR_EOK : MGMT_ERR_ENOMEM;
}

/**
 * Command handler: stat show
 */
static int
stat_mgmt_show(struct mgmt_ctxt *ctxt)
{
	char stat_name[STAT_MGMT_MAX_NAME_LEN];
	CborError err;
	zcbor_state_t *zse = ctxt->cnbe->zs;
	bool ok;
	size_t counter = 0;

	struct cbor_attr_t attrs[] = {
		{
			.attribute = "name",
			.type = CborAttrTextStringType,
			.addr.string = stat_name,
			.len = sizeof(stat_name)
		},
		{ NULL },
	};

	err = cbor_read_object(&ctxt->it, attrs);
	if (err != 0) {
		return MGMT_ERR_EINVAL;
	}

	if (stat_mgmt_count(stat_name, &counter) != 0) {
		return MGMT_ERR_EUNKNOWN;
	}

	ok = zcbor_tstr_put_lit(zse, "rc")		&&
	     zcbor_int32_put(zse, MGMT_ERR_EOK)		&&
	     zcbor_tstr_put_lit(zse, "name")		&&
	     zcbor_tstr_put_term(zse, stat_name)	&&
	     zcbor_tstr_put_lit(zse, "fields")		&&
	     zcbor_map_start_encode(zse, counter);

	if (ok) {
		int rc = stat_mgmt_foreach_entry(zse, stat_name, stat_mgmt_cb_encode);

		if (rc != MGMT_ERR_EOK) {
			return rc;
		}
	} else {
		return MGMT_ERR_ENOMEM;
	}

	ok = zcbor_map_end_encode(zse, counter);
	return ok ? MGMT_ERR_EOK : MGMT_ERR_ENOMEM;
}

/**
 * Command handler: stat list
 */
static int
stat_mgmt_list(struct mgmt_ctxt *ctxt)
{
	const struct stats_hdr *cur = NULL;
	zcbor_state_t *zse = ctxt->cnbe->zs;
	bool ok;
	size_t counter = 0;

	do {
		cur = stats_group_get_next(cur);
		if (cur != NULL) {
			counter++;
		}
	} while (cur != NULL);

	ok = zcbor_tstr_put_lit(zse, "rc")		&&
	     zcbor_int32_put(zse, MGMT_ERR_EOK)		&&
	     zcbor_tstr_put_lit(zse, "stat_list")	&&
	     zcbor_list_start_encode(zse, counter);

	if (!ok) {
		return MGMT_ERR_ENOMEM;
	}
	/* Iterate the list of stat groups, encoding each group's name in the CBOR
	 * array.
	 */
	cur = NULL;
	do {
		cur = stats_group_get_next(cur);
		if (cur != NULL) {
			ok = zcbor_tstr_put_term(zse, cur->s_name);
		}
	} while (ok && cur != NULL);

	if (!ok || !zcbor_list_end_encode(zse, counter)) {
		return MGMT_ERR_ENOMEM;
	}

	return 0;
}

static struct mgmt_handler stat_mgmt_handlers[] = {
	[STAT_MGMT_ID_SHOW] = { stat_mgmt_show, NULL },
	[STAT_MGMT_ID_LIST] = { stat_mgmt_list, NULL },
};

#define STAT_MGMT_HANDLER_CNT ARRAY_SIZE(stat_mgmt_handlers)

static struct mgmt_group stat_mgmt_group = {
	.mg_handlers = stat_mgmt_handlers,
	.mg_handlers_count = STAT_MGMT_HANDLER_CNT,
	.mg_group_id = MGMT_GROUP_ID_STAT,
};

void
stat_mgmt_register_group(void)
{
	mgmt_register_group(&stat_mgmt_group);
}
