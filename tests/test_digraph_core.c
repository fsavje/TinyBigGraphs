/* ==============================================================================
 * scclust -- A C library for size constrained clustering
 * https://github.com/fsavje/scclust
 *
 * Copyright (C) 2015-2016  Fredrik Savje -- http://fredriksavje.com
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see http://www.gnu.org/licenses/
 * ============================================================================== */

#include "test_suite.h"
#include "../src/digraph_core.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/scclust.h"
#include "../src/config.h"

// ISCC_DPID_MAX should be UINT32_MAX
// ISCC_ARCI_MAX should be UINT32_MAX
// SIZE_MAX is redefined to (UINT32_MAX - 10) by test_suite.h


void scc_ut_free_digraph(void** state)
{
	(void) state;

	iscc_Digraph dg1 = {
		.vertices = 123,
		.max_arcs = 1234,
		.head = malloc(sizeof(iscc_Dpid[1234])),
		.tail_ptr = malloc(sizeof(iscc_Arci[124])),
	};

	iscc_Digraph dg2 = {
		.vertices = 123,
		.max_arcs = 0,
		.head = NULL,
		.tail_ptr = malloc(sizeof(iscc_Arci[124])),
	};

	iscc_Digraph dg3 = {
		.vertices = 123,
		.max_arcs = 1234,
		.head = NULL,
		.tail_ptr = malloc(sizeof(iscc_Arci[124])),
	};

	iscc_Digraph dg4 = {
		.vertices = 123,
		.max_arcs = 1234,
		.head = malloc(sizeof(iscc_Dpid[1234])),
		.tail_ptr = NULL,
	};

	iscc_Digraph dg5 = ISCC_NULL_DIGRAPH;

	iscc_free_digraph(NULL);

	iscc_free_digraph(&dg1);
	assert_memory_equal(&dg1, &ISCC_NULL_DIGRAPH, sizeof(iscc_Digraph));

	iscc_free_digraph(&dg2);
	assert_memory_equal(&dg2, &ISCC_NULL_DIGRAPH, sizeof(iscc_Digraph));

	iscc_free_digraph(&dg3);
	assert_memory_equal(&dg3, &ISCC_NULL_DIGRAPH, sizeof(iscc_Digraph));

	iscc_free_digraph(&dg4);
	assert_memory_equal(&dg4, &ISCC_NULL_DIGRAPH, sizeof(iscc_Digraph));

	iscc_free_digraph(&dg5);
	assert_memory_equal(&dg5, &ISCC_NULL_DIGRAPH, sizeof(iscc_Digraph));
}


void scc_ut_digraph_is_initialized(void** state)
{
	(void) state;

	iscc_Dpid heads[100];
	iscc_Arci tails[11];

	iscc_Digraph dg = {
		.vertices = 10,
		.max_arcs = 100,
		.head = heads,
		.tail_ptr = tails,
	};

	assert_false(iscc_digraph_is_initialized(NULL));
	
	assert_false(iscc_digraph_is_initialized(&ISCC_NULL_DIGRAPH));

	assert_true(iscc_digraph_is_initialized(&dg));

	dg.max_arcs = 0;
	dg.head = NULL;
	assert_true(iscc_digraph_is_initialized(&dg));
	dg.max_arcs = 100;
	dg.head = heads;

	dg.tail_ptr = NULL;
	assert_false(iscc_digraph_is_initialized(&dg));
	dg.tail_ptr = tails;

	dg.vertices = ((size_t) UINT32_MAX) + 1;
	assert_false(iscc_digraph_is_initialized(&dg));
	dg.vertices = 10;
	
	dg.max_arcs = ((size_t) UINT32_MAX) + 1;
	assert_false(iscc_digraph_is_initialized(&dg));
	dg.max_arcs = 100;

	dg.max_arcs = 0;
	assert_false(iscc_digraph_is_initialized(&dg));
	dg.max_arcs = 100;
	
	dg.max_arcs = 0;
	assert_false(iscc_digraph_is_initialized(&dg));
	dg.max_arcs = 100;

	dg.head = NULL;
	assert_false(iscc_digraph_is_initialized(&dg));
	dg.head = heads;
}


void scc_ut_init_digraph(void** state)
{
	(void) state;

	iscc_Digraph dg1;
	scc_ErrorCode ec1 = iscc_init_digraph(100, ((uint64_t) UINT32_MAX) + 1, &dg1);
	assert_int_equal(ec1, SCC_ER_TOO_LARGE_DIGRAPH);
	
	iscc_Digraph dg2;
	scc_ErrorCode ec2 = iscc_init_digraph(100, ((uint64_t) UINT32_MAX) - 5, &dg2);
	assert_int_equal(ec2, SCC_ER_TOO_LARGE_DIGRAPH);

	iscc_Digraph dg3;
	scc_ErrorCode ec3 = iscc_init_digraph(100, 1000, &dg3);
	assert_true(iscc_digraph_is_initialized(&dg3));
	assert_int_equal(dg3.vertices, 100);
	assert_int_equal(dg3.max_arcs, 1000);
	assert_non_null(dg3.head);
	assert_non_null(dg3.tail_ptr);
	assert_int_equal(ec3, SCC_ER_OK);
	
	iscc_Digraph dg4;
	scc_ErrorCode ec4 = iscc_init_digraph(100, 0, &dg4);
	assert_true(iscc_digraph_is_initialized(&dg4));
	assert_int_equal(dg4.vertices, 100);
	assert_int_equal(dg4.max_arcs, 0);
	assert_null(dg4.head);
	assert_non_null(dg4.tail_ptr);
	assert_int_equal(ec4, SCC_ER_OK);

	iscc_free_digraph(&dg3);
	iscc_free_digraph(&dg4);
}


void scc_ut_empty_digraph(void** state)
{
	(void) state;

	iscc_Digraph dg1;
	scc_ErrorCode ec1 = iscc_empty_digraph(100, ((uint64_t) UINT32_MAX) + 1, &dg1);
	assert_int_equal(ec1, SCC_ER_TOO_LARGE_DIGRAPH);
	
	iscc_Digraph dg2;
	scc_ErrorCode ec2 = iscc_empty_digraph(100, ((uint64_t) UINT32_MAX) - 5, &dg2);
	assert_int_equal(ec2, SCC_ER_TOO_LARGE_DIGRAPH);

	iscc_Digraph dg3;
	scc_ErrorCode ec3 = iscc_empty_digraph(100, 1000, &dg3);
	assert_true(iscc_digraph_is_initialized(&dg3));
	assert_int_equal(dg3.vertices, 100);
	assert_int_equal(dg3.max_arcs, 1000);
	assert_non_null(dg3.head);
	assert_non_null(dg3.tail_ptr);
	for (size_t i = 0; i < 101; ++i) {
		assert_int_equal(dg3.tail_ptr[i], 0);
	}
	assert_int_equal(ec3, SCC_ER_OK);
	
	iscc_Digraph dg4;
	scc_ErrorCode ec4 = iscc_empty_digraph(100, 0, &dg4);
	assert_true(iscc_digraph_is_initialized(&dg4));
	assert_int_equal(dg4.vertices, 100);
	assert_int_equal(dg4.max_arcs, 0);
	assert_null(dg4.head);
	assert_non_null(dg4.tail_ptr);
	for (size_t i = 0; i < 101; ++i) {
		assert_int_equal(dg4.tail_ptr[i], 0);
	}
	assert_int_equal(ec4, SCC_ER_OK);

	iscc_free_digraph(&dg3);
	iscc_free_digraph(&dg4);
}


void scc_ut_change_arc_storage(void** state)
{
	(void) state;

	iscc_Arci tails[6] = { 0, 1, 2, 3, 4, 5 };
	iscc_Arci tails_ref[6] = { 0, 1, 2, 3, 4, 5 };
	iscc_Arci tails_zero[6] = { 0, 0, 0, 0, 0, 0 };

	iscc_Digraph dg = {
		.vertices = 5,
		.max_arcs = 10,
		.head = malloc(sizeof(iscc_Dpid[10])),
		.tail_ptr = tails,
	};

	assert_true(iscc_digraph_is_initialized(&dg));
	scc_ErrorCode ec1 = iscc_change_arc_storage(&dg, ((uint64_t) UINT32_MAX) + 1);
	assert_int_equal(dg.vertices, 5);
	assert_int_equal(dg.max_arcs, 10);
	assert_non_null(dg.head);
	assert_non_null(dg.tail_ptr);
	assert_memory_equal(dg.tail_ptr, tails_ref, 6 * sizeof(iscc_Arci));
	assert_int_equal(ec1, SCC_ER_TOO_LARGE_DIGRAPH);

	assert_true(iscc_digraph_is_initialized(&dg));
	scc_ErrorCode ec2 = iscc_change_arc_storage(&dg, ((uint64_t) UINT32_MAX) - 5);
	assert_int_equal(dg.vertices, 5);
	assert_int_equal(dg.max_arcs, 10);
	assert_non_null(dg.head);
	assert_non_null(dg.tail_ptr);
	assert_memory_equal(dg.tail_ptr, tails_ref, 6 * sizeof(iscc_Arci));
	assert_int_equal(ec2, SCC_ER_TOO_LARGE_DIGRAPH);

	assert_true(iscc_digraph_is_initialized(&dg));
	scc_ErrorCode ec3 = iscc_change_arc_storage(&dg, 10);
	assert_int_equal(dg.vertices, 5);
	assert_int_equal(dg.max_arcs, 10);
	assert_non_null(dg.head);
	assert_non_null(dg.tail_ptr);
	assert_memory_equal(dg.tail_ptr, tails_ref, 6 * sizeof(iscc_Arci));
	assert_int_equal(ec3, SCC_ER_OK);

	assert_true(iscc_digraph_is_initialized(&dg));
	scc_ErrorCode ec4 = iscc_change_arc_storage(&dg, 100);
	assert_int_equal(dg.vertices, 5);
	assert_int_equal(dg.max_arcs, 100);
	assert_non_null(dg.head);
	assert_non_null(dg.tail_ptr);
	assert_memory_equal(dg.tail_ptr, tails_ref, 6 * sizeof(iscc_Arci));
	assert_int_equal(ec4, SCC_ER_OK);

	dg.tail_ptr = tails_zero;
	assert_true(iscc_digraph_is_initialized(&dg));
	scc_ErrorCode ec5 = iscc_change_arc_storage(&dg, 0);
	assert_int_equal(dg.vertices, 5);
	assert_int_equal(dg.max_arcs, 0);
	assert_null(dg.head);
	assert_non_null(dg.tail_ptr);
	assert_int_equal(ec5, SCC_ER_OK);
}


int main(void)
{
	const struct CMUnitTest test_cases[] = {
		cmocka_unit_test(scc_ut_free_digraph),
		cmocka_unit_test(scc_ut_digraph_is_initialized),
		cmocka_unit_test(scc_ut_init_digraph),
		cmocka_unit_test(scc_ut_empty_digraph),
		cmocka_unit_test(scc_ut_change_arc_storage),
	};
	
	return cmocka_run_group_tests_name("digraph_core.c", test_cases, NULL, NULL);
}
