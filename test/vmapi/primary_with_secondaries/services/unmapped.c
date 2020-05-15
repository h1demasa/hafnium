/*
 * Copyright 2019 The Hafnium Authors.
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

#include "hf/arch/vm/interrupts.h"

#include "hf/mm.h"
#include "hf/std.h"

#include "vmapi/hf/call.h"

#include "../sysregs.h"
#include "test/hftest.h"
#include "test/vmapi/exception_handler.h"

alignas(PAGE_SIZE) static uint8_t pages[2 * PAGE_SIZE];

TEST_SERVICE(data_unmapped)
{
	/* Not using NULL so static analysis doesn't complain. */
	int *p = (int *)1;
	exception_setup(NULL, exception_handler_yield_data_abort);
	*p = 12;
	FAIL("Exception not generated by invalid access.");
}

TEST_SERVICE(straddling_data_unmapped)
{
	void *send_buf = SERVICE_SEND_BUFFER();
	/* Give some memory to the primary VM so that it's unmapped. */
	struct ffa_memory_region_constituent constituents[] = {
		{.address = (uint64_t)(&pages[PAGE_SIZE]), .page_count = 1},
	};
	uint32_t msg_size;
	EXPECT_EQ(ffa_memory_region_init(
			  send_buf, HF_MAILBOX_SIZE, hf_vm_get_id(),
			  HF_PRIMARY_VM_ID, constituents,
			  ARRAY_SIZE(constituents), 0, 0,
			  FFA_DATA_ACCESS_NOT_SPECIFIED,
			  FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
			  FFA_MEMORY_NORMAL_MEM, FFA_MEMORY_CACHE_WRITE_BACK,
			  FFA_MEMORY_OUTER_SHAREABLE, NULL, &msg_size),
		  0);
	exception_setup(NULL, exception_handler_yield_data_abort);

	EXPECT_EQ(ffa_mem_donate(msg_size, msg_size).func, FFA_SUCCESS_32);

	*(volatile uint64_t *)(&pages[PAGE_SIZE - 6]);
	FAIL("Exception not generated by invalid access.");
}

TEST_SERVICE(instruction_unmapped)
{
	/* Not using NULL so static analysis doesn't complain. */
	int (*f)(void) = (int (*)(void))4;
	exception_setup(NULL, exception_handler_yield_instruction_abort);
	f();
	FAIL("Exception not generated by invalid access.");
}
