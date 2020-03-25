/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hf/fdt_handler.h"

#include "hf/check.h"
#include "hf/cpu.h"
#include "hf/dlog.h"
#include "hf/fdt.h"
#include "hf/mm.h"
#include "hf/std.h"

/**
 * Finds the memory region where initrd is stored.
 */
bool fdt_find_initrd(const struct fdt *fdt, paddr_t *begin, paddr_t *end)
{
	struct fdt_node n;
	uint64_t initrd_begin;
	uint64_t initrd_end;

	if (!fdt_find_node(fdt, "/chosen", &n)) {
		dlog_error("Unable to find '/chosen'\n");
		return false;
	}

	if (!fdt_read_number(&n, FDT_PROP_INITRD_START, &initrd_begin)) {
		dlog_error("Unable to read " FDT_PROP_INITRD_START "\n");
		return false;
	}

	if (!fdt_read_number(&n, FDT_PROP_INITRD_END, &initrd_end)) {
		dlog_error("Unable to read " FDT_PROP_INITRD_END "\n");
		return false;
	}

	*begin = pa_init(initrd_begin);
	*end = pa_init(initrd_end);

	return true;
}

bool fdt_find_cpus(const struct fdt *fdt, cpu_id_t *cpu_ids, size_t *cpu_count)
{
	static const struct string str_cpu = STRING_INIT("cpu");
	struct fdt_node n;
	size_t addr_size;

	*cpu_count = 0;

	if (!fdt_find_node(fdt, "/cpus", &n)) {
		dlog_error("Unable to find '/cpus'\n");
		return false;
	}

	if (!fdt_address_size(&n, &addr_size)) {
		return false;
	}

	if (!fdt_first_child(&n)) {
		return false;
	}

	do {
		struct memiter data;

		if (!fdt_read_property(&n, "device_type", &data) ||
		    !string_eq(&str_cpu, &data) ||
		    !fdt_read_property(&n, "reg", &data)) {
			continue;
		}

		/* Get all entries for this CPU. */
		while (memiter_size(&data)) {
			uint64_t value;

			if (*cpu_count >= MAX_CPUS) {
				dlog_error("Found more than %d CPUs\n",
					   MAX_CPUS);
				return false;
			}

			if (!fdt_parse_number(&data, addr_size, &value)) {
				dlog_error("Could not parse CPU id\n");
				return false;
			}
			cpu_ids[(*cpu_count)++] = value;
		}
	} while (fdt_next_sibling(&n));

	return true;
}

bool fdt_find_memory_ranges(const struct fdt *fdt, struct string *device_type,
			    struct mem_range *mem_ranges,
			    size_t *mem_ranges_count, size_t mem_range_limit)
{
	struct fdt_node n;
	size_t addr_size;
	size_t size_size;
	size_t mem_range_index = 0;

	if (!fdt_find_node(fdt, "/", &n) || !fdt_address_size(&n, &addr_size) ||
	    !fdt_size_size(&n, &size_size)) {
		return false;
	}

	/* Look for nodes with the device_type set to `device_type`. */
	if (!fdt_first_child(&n)) {
		return false;
	}

	do {
		struct memiter data;

		if (!fdt_read_property(&n, "device_type", &data) ||
		    !string_eq(device_type, &data) ||
		    !fdt_read_property(&n, "reg", &data)) {
			continue;
		}

		/* Traverse all memory ranges within this node. */
		while (memiter_size(&data)) {
			uintpaddr_t addr;
			size_t len;

			CHECK(fdt_parse_number(&data, addr_size, &addr));
			CHECK(fdt_parse_number(&data, size_size, &len));

			if (mem_range_index < mem_range_limit) {
				mem_ranges[mem_range_index].begin =
					pa_init(addr);
				mem_ranges[mem_range_index].end =
					pa_init(addr + len);
				++mem_range_index;
			} else {
				dlog_error(
					"Found %s range %u in FDT but only %u "
					"supported, ignoring additional range "
					"of size %u.\n",
					string_data(device_type),
					mem_range_index, mem_range_limit, len);
			}
		}
	} while (fdt_next_sibling(&n));
	*mem_ranges_count = mem_range_index;

	return true;
}

bool fdt_map(struct fdt *fdt, struct mm_stage1_locked stage1_locked,
	     paddr_t fdt_addr, struct mpool *ppool)
{
	const void *fdt_ptr;
	size_t fdt_len;

	/* Map the fdt header in. */
	fdt_ptr = mm_identity_map(stage1_locked, fdt_addr,
				  pa_add(fdt_addr, FDT_V17_HEADER_SIZE),
				  MM_MODE_R, ppool);
	if (!fdt_ptr) {
		dlog_error("Unable to map FDT header.\n");
		return NULL;
	}

	if (!fdt_size_from_header(fdt_ptr, &fdt_len)) {
		dlog_error("FDT failed header validation.\n");
		goto fail;
	}

	/* Map the rest of the fdt in. */
	fdt_ptr = mm_identity_map(stage1_locked, fdt_addr,
				  pa_add(fdt_addr, fdt_len), MM_MODE_R, ppool);
	if (!fdt_ptr) {
		dlog_error("Unable to map full FDT.\n");
		goto fail;
	}

	if (!fdt_init_from_ptr(fdt, fdt_ptr, fdt_len)) {
		dlog_error("FDT failed validation.\n");
		goto fail_full;
	}

	return true;

fail_full:
	mm_unmap(stage1_locked, fdt_addr, pa_add(fdt_addr, fdt_len), ppool);
	return false;

fail:
	mm_unmap(stage1_locked, fdt_addr, pa_add(fdt_addr, FDT_V17_HEADER_SIZE),
		 ppool);
	return false;
}

bool fdt_unmap(struct fdt *fdt, struct mm_stage1_locked stage1_locked,
	       struct mpool *ppool)
{
	paddr_t begin = pa_from_va(va_from_ptr(fdt_base(fdt)));
	paddr_t end = pa_add(begin, fdt_size(fdt));

	if (!mm_unmap(stage1_locked, begin, end, ppool)) {
		return false;
	}

	/* Invalidate pointer to the buffer. */
	fdt_fini(fdt);
	return true;
}
