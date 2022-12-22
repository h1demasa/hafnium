/*
 * Copyright 2023 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#include "sp_helpers.h"

#include "partition_services.h"

#define ITERATIONS_PER_MS 15000

uint64_t sp_sleep_active_wait(uint32_t ms)
{
	sp_wait_loop(ms * ITERATIONS_PER_MS);
	return ms;
}
