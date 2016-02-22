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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * ============================================================================== */


#include "digraph_operations.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "../include/scclust.h"
#include "digraph_core.h"
#include "error.h"


// ==============================================================================
// Internal function prototypes
// ==============================================================================

static inline size_t iscc_do_union(size_t vertices,
                                   size_t num_dgs,
                                   const iscc_Digraph dgs[restrict static num_dgs],
                                   scc_Dpid row_markers[restrict static vertices],
                                   bool write,
                                   scc_Arci out_tail_ptr[restrict],
                                   scc_Dpid out_head[restrict]);

static inline size_t iscc_do_union_and_delete(size_t vertices,
                                              size_t num_dgs,
                                              const iscc_Digraph dgs[restrict static num_dgs],
                                              scc_Dpid row_markers[restrict static vertices],
                                              const bool tails_to_keep[restrict static vertices],
                                              bool write,
                                              scc_Arci out_tail_ptr[restrict],
                                              scc_Dpid out_head[restrict]);

static inline size_t iscc_do_difference(size_t vertices,
                                        const scc_Arci minuend_tail_ptr[static vertices + 1],
                                        const scc_Dpid* minuend_head,
                                        const scc_Arci subtrahend_tail_ptr[static vertices + 1],
                                        const scc_Dpid* subtrahend_head,
                                        size_t max_out_degree,
                                        scc_Dpid row_markers[restrict static vertices],
                                        bool write,
                                        scc_Arci out_tail_ptr[restrict],
                                        scc_Dpid out_head[restrict]);

static inline size_t iscc_do_adjacency_product(size_t vertices,
                                               const scc_Arci dg_a_tail_ptr[static vertices + 1],
                                               const scc_Dpid* dg_a_head,
                                               const scc_Arci dg_b_tail_ptr[static vertices + 1],
                                               const scc_Dpid* dg_b_head,
                                               scc_Dpid row_markers[restrict static vertices],
                                               bool force_loops,
                                               bool ignore_loops,
                                               bool write,
                                               scc_Arci out_tail_ptr[restrict],
                                               scc_Dpid out_head[restrict]);


// ==============================================================================
// External function implementations
// ==============================================================================

scc_ErrorCode iscc_delete_loops_check_error(iscc_Digraph* dg)
{
	assert(iscc_digraph_is_initialized(dg));

	scc_Arci head_write = 0;
	assert(dg->vertices < SCC_DPID_MAX);
	const scc_Dpid vertices = (scc_Dpid) dg->vertices; // If `scc_Dpid` is signed 
	for (scc_Dpid v = 0; v < vertices; ++v) {
		const scc_Dpid* v_arc = dg->head + dg->tail_ptr[v];
		const scc_Dpid* const v_arc_stop = dg->head + dg->tail_ptr[v + 1];
		dg->tail_ptr[v] = head_write;

		for (; v_arc != v_arc_stop; ++v_arc) {
			if (*v_arc != v) {
				dg->head[head_write] = *v_arc;
				++head_write;
			}
		}
	}
	dg->tail_ptr[dg->vertices] = head_write;

	return iscc_no_error();
}


scc_ErrorCode iscc_digraph_union(const size_t num_in_dgs,
                                 const iscc_Digraph in_dgs[const static num_in_dgs],
                                 iscc_Digraph* const out_dg)
{
	assert(num_in_dgs > 0);
	assert(in_dgs != NULL);
	assert(out_dg != NULL);
	assert(iscc_digraph_is_initialized(&in_dgs[0]));

	const size_t vertices = in_dgs[0].vertices;

	// Try greedy memory count first
	size_t out_arcs_write = 0;
	for (size_t i = 0; i < num_in_dgs; ++i) {
		assert(iscc_digraph_is_initialized(&in_dgs[i]));
		assert(in_dgs[0].vertices == vertices);
		out_arcs_write += in_dgs[i].tail_ptr[vertices];
	}

	scc_Dpid* const row_markers = malloc(sizeof(scc_Dpid[vertices]));
	if (row_markers == NULL) return iscc_make_error(SCC_ER_NO_MEMORY);

	scc_ErrorCode ec;
	if (iscc_init_digraph(vertices, out_arcs_write, out_dg) != SCC_ER_OK) {
		// Could not allocate digraph with `out_arcs_write' arcs.
		// Do correct (but slow) memory count by doing
		// union without writing.
		out_arcs_write = iscc_do_union(vertices,
		                               num_in_dgs, in_dgs,
		                               row_markers,
		                               false, NULL, NULL);

		// Try again. If fail, give up.
		if ((ec = iscc_init_digraph(vertices, out_arcs_write, out_dg)) != SCC_ER_OK) {
			free(row_markers);
			return ec;
		}
	}

	out_arcs_write = iscc_do_union(vertices, 
	                               num_in_dgs, in_dgs,
	                               row_markers,
	                               true, out_dg->tail_ptr, out_dg->head);

	free(row_markers);

	if ((ec = iscc_change_arc_storage(out_dg, out_arcs_write)) != SCC_ER_OK) {
		iscc_free_digraph(out_dg);
		return ec;
	}

	return iscc_no_error();
}


scc_ErrorCode iscc_digraph_union_and_delete(const size_t num_in_dgs,
                                            const iscc_Digraph in_dgs[const static num_in_dgs],
                                            const bool tails_to_keep[const],
                                            iscc_Digraph* const out_dg)
{
	assert(num_in_dgs > 0);
	assert(in_dgs != NULL);
	assert(tails_to_keep != NULL);
	assert(out_dg != NULL);
	assert(iscc_digraph_is_initialized(&in_dgs[0]));

	const size_t vertices = in_dgs[0].vertices;

	// Try greedy memory count first
	size_t out_arcs_write = 0;
	for (size_t i = 0; i < num_in_dgs; ++i) {
		assert(iscc_digraph_is_initialized(&in_dgs[i]));
		assert(in_dgs[0].vertices == vertices);
		out_arcs_write += in_dgs[i].tail_ptr[vertices];
	}

	scc_Dpid* const row_markers = malloc(sizeof(scc_Dpid[vertices]));
	if (row_markers == NULL) return iscc_make_error(SCC_ER_NO_MEMORY);

	scc_ErrorCode ec;
	if (iscc_init_digraph(vertices, out_arcs_write, out_dg) != SCC_ER_OK) {
		// Could not allocate digraph with `out_arcs_write' arcs.
		// Do correct (but slow) memory count by doing
		// union without writing.
		out_arcs_write = iscc_do_union_and_delete(vertices,
		                                          num_in_dgs, in_dgs,
		                                          row_markers, tails_to_keep,
		                                          false, NULL, NULL);

		// Try again. If fail, give up.
		if ((ec = iscc_init_digraph(vertices, out_arcs_write, out_dg)) != SCC_ER_OK) {
			free(row_markers);
			return ec;
		}
	}

	out_arcs_write = iscc_do_union_and_delete(vertices,
	                                          num_in_dgs, in_dgs,
	                                          row_markers, tails_to_keep,
	                                          true, out_dg->tail_ptr, out_dg->head);

	free(row_markers);

	if ((ec = iscc_change_arc_storage(out_dg, out_arcs_write)) != SCC_ER_OK) {
		iscc_free_digraph(out_dg);
		return ec;
	}

	return iscc_no_error();
}


scc_ErrorCode iscc_digraph_difference(const iscc_Digraph* const minuend_dg,
                                      const iscc_Digraph* const subtrahend_dg,
                                      const size_t max_out_degree,
                                      iscc_Digraph* const out_dg)
{
	assert(iscc_digraph_is_initialized(minuend_dg));
	assert(iscc_digraph_is_initialized(subtrahend_dg));
	assert(minuend_dg->vertices > 0);
	assert(minuend_dg->vertices == subtrahend_dg->vertices);
	assert(out_dg != NULL);

	const size_t vertices = in_dgs[0].vertices;

	scc_Dpid* const row_markers = malloc(sizeof(scc_Dpid[vertices]));
	if (row_markers == NULL) return iscc_make_error(SCC_ER_NO_MEMORY);

	// Try greedy memory count first
	size_t out_arcs_write = minuend_dg->tail_ptr[minuend_dg->vertices];

	scc_ErrorCode ec;
	if (iscc_init_digraph(vertices, out_arcs_write, out_dg) != SCC_ER_OK) {
		// Could not allocate digraph with `out_arcs_write' arcs.
		// Do correct (but slow) memory count by doing
		// union without writing.
		out_arcs_write = iscc_do_difference(vertices,
                                            minuend_dg->tail_ptr, minuend_dg->head,
                                            subtrahend_dg->tail_ptr, subtrahend_dg->head,
                                            max_out_degree,
                                            row_markers,
                                            false, NULL, NULL);

		// Try again. If fail, give up.
		if ((ec = iscc_init_digraph(vertices, out_arcs_write, out_dg)) != SCC_ER_OK) {
			free(row_markers);
			return ec;
		}
	}

	out_arcs_write = iscc_do_difference(vertices,
                                        minuend_dg->tail_ptr, minuend_dg->head,
                                        subtrahend_dg->tail_ptr, subtrahend_dg->head,
                                        max_out_degree,
                                        row_markers,
                                        true, out_dg->tail_ptr, out_dg->head);

	free(row_markers);

	if ((ec = iscc_change_arc_storage(out_dg, out_arcs_write)) != SCC_ER_OK) {
		iscc_free_digraph(out_dg);
		return ec;
	}

	return iscc_no_error();
}


/*
// Alternative implementation, quicker and less memory. But changes the order of the arcs.
// Most rewrite the tests for the exclusion updating find seed method if used.
iscc_Digraph iscc_digraph_transpose(const scc_Digraph* const dg)
{
	if (!scc_digraph_is_initialized(dg)) return SCC_NULL_DIGRAPH;
	if (dg->vertices == 0) return scc_empty_digraph(0, 0);

	scc_Digraph dg_out = scc_empty_digraph(dg->vertices, dg->tail_ptr[dg->vertices]);
	if (!scc_digraph_is_initialized(&dg_out)) return SCC_NULL_DIGRAPH;

	const scc_Vid* const arc_c_stop = dg->head + dg->tail_ptr[dg->vertices];
	for (const scc_Vid* arc_c = dg->head;
	        arc_c != arc_c_stop; ++arc_c) {
		++dg_out.tail_ptr[*arc_c];
	}

	for (size_t v = 0; v < dg->vertices; ++v) {
		dg_out.tail_ptr[v + 1] += dg_out.tail_ptr[v];
	}

	assert(dg->vertices < SCC_VID_MAX);
	const scc_Vid vertices = (scc_Vid) dg->vertices; // If `scc_Vid` is signed 
	for (scc_Vid v = 0; v < vertices; ++v) {
		const scc_Vid* const arc_stop = dg->head + dg->tail_ptr[v + 1];
		for (const scc_Vid* arc = dg->head + dg->tail_ptr[v];
		        arc != arc_stop; ++arc) {
			--dg_out.tail_ptr[*arc];
			dg_out.head[dg_out.tail_ptr[*arc]] = v;
		}
	}

	return dg_out;
}*/
scc_ErrorCode iscc_digraph_transpose(const iscc_Digraph* const in_dg,
                                     iscc_Digraph* const out_dg)
{
	assert(iscc_digraph_is_initialized(in_dg));
	assert(in_dg->vertices > 0);
	assert(out_dg != NULL);

	scc_ErrorCode ec;
	if ((ec = iscc_empty_digraph(in_dg->vertices, in_dg->tail_ptr[in_dg->vertices], out_dg)) != SCC_ER_OK) {
		return ec;
	}

	const scc_Dpid* const arc_c_stop = in_dg->head + in_dg->tail_ptr[in_dg->vertices];
	for (const scc_Dpid* arc_c = in_dg->head;
	        arc_c != arc_c_stop; ++arc_c) {
		++out_dg->tail_ptr[*arc_c];
	}

	for (size_t v = 0; v < in_dg->vertices; ++v) {
		out_dg->tail_ptr[v + 1] += out_dg->tail_ptr[v];
	}

	assert(in_dg->vertices < SCC_DPID_MAX);
	const scc_Dpid vertices = (scc_Dpid) in_dg->vertices; // If `scc_Dpid` is signed 
	for (scc_Dpid v = 0; v < vertices; ++v) {
		const scc_Dpid* const arc_stop = in_dg->head + in_dg->tail_ptr[v + 1];
		for (const scc_Dpid* arc = in_dg->head + in_dg->tail_ptr[v];
		        arc != arc_stop; ++arc) {
			--out_dg->tail_ptr[*arc];
			out_dg->head[out_dg->tail_ptr[*arc]] = v;
		}
	}

	return iscc_no_error();
}


scc_ErrorCode iscc_adjacency_product(const iscc_Digraph* const in_dg_a,
                                     const iscc_Digraph* const in_dg_b,
                                     const bool force_loops,
                                     const bool ignore_loops,
                                     iscc_Digraph* const out_dg)
{
	assert(iscc_digraph_is_initialized(in_dg_a));
	assert(iscc_digraph_is_initialized(in_dg_b));
	assert(in_dg_a->vertices > 0);
	assert(in_dg_a->vertices == in_dg_b->vertices);
	assert(!force_loops || !ignore_loops);
	assert(out_dg != NULL);

	const size_t vertices = in_dg_a->vertices;
	size_t out_arcs_write = 0;

	scc_Dpid* const row_markers = malloc(sizeof(scc_Dpid[vertices]));
	if (row_markers == NULL) return iscc_make_error(SCC_ER_NO_MEMORY);

	// Try greedy memory count first
	assert(vertices < SCC_DPID_MAX);
	const scc_Dpid vertices_dpid = (scc_Dpid) vertices; // If `scc_Dpid` is signed 
	for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
		if (force_loops) {
			out_arcs_write += in_dg_b->tail_ptr[v + 1] - in_dg_b->tail_ptr[v];
		}
		const scc_Dpid* const arc_a_stop = in_dg_a->head + in_dg_a->tail_ptr[v + 1];
		for (const scc_Dpid* arc_a = in_dg_a->head + in_dg_a->tail_ptr[v];
		        arc_a != arc_a_stop; ++arc_a) {
			if ((*arc_a == v) && (force_loops || ignore_loops)) continue;
			out_arcs_write += in_dg_b->tail_ptr[*arc_a + 1] - in_dg_b->tail_ptr[*arc_a];
		}
	}


	scc_ErrorCode ec;
	if (iscc_init_digraph(vertices, out_arcs_write, out_dg) != SCC_ER_OK) {
		// Could not allocate digraph with `out_arcs_write' arcs.
		// Do correct (but slow) memory count by doing
		// doing product without writing.
		out_arcs_write = iscc_do_adjacency_product(vertices,
		                                           in_dg_a->tail_ptr, in_dg_a->head,
		                                           in_dg_b->tail_ptr, in_dg_b->head,
		                                           row_markers,
		                                           force_loops, ignore_loops,
		                                           false, NULL, NULL);

		// Try again. If fail, give up.
		if ((ec = iscc_init_digraph(vertices, out_arcs_write, out_dg)) != SCC_ER_OK) {
			free(row_markers);
			return ec;
		}
	}


	out_arcs_write = iscc_do_adjacency_product(vertices,
	                                           in_dg_a->tail_ptr, in_dg_a->head,
	                                           in_dg_b->tail_ptr, in_dg_b->head,
	                                           row_markers,
	                                           force_loops, ignore_loops,
	                                           true, out_dg->tail_ptr, out_dg->head);

	free(row_markers);

	if ((ec = iscc_change_arc_storage(out_dg, out_arcs_write)) != SCC_ER_OK) {
		iscc_free_digraph(out_dg);
		return ec;
	}

	return iscc_no_error();
}


// ==============================================================================
// Internal function implementations 
// ==============================================================================

static inline size_t iscc_do_union(const size_t vertices,
                                   const size_t num_dgs,
                                   const iscc_Digraph dgs[restrict const static num_dgs],
                                   scc_Dpid row_markers[restrict const static vertices],
                                   const bool write,
                                   scc_Arci out_tail_ptr[restrict const],
                                   scc_Dpid out_head[restrict const])
{
	assert(vertices > 0);
	assert(num_dgs > 0);
	assert(dgs != NULL);
	assert(row_markers != NULL);

	size_t counter = 0;
	for (size_t v = 0; v < vertices; ++v) {
		row_markers[v] = SCC_DPID_NA;
	}

	assert(vertices < SCC_DPID_MAX);
	const scc_Dpid vertices_dpid = (scc_Dpid) vertices; // If `scc_Dpid` is signed 

	if (write) {
		assert(out_tail_ptr != NULL);
		assert(out_head != NULL);

		out_tail_ptr[0] = 0;
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			row_markers[v] = v;
			for (size_t i = 0; i < num_dgs; ++i) {
				const scc_Dpid* const arc_i_stop = dgs[i].head + dgs[i].tail_ptr[v + 1];
				for (const scc_Dpid* arc_i = dgs[i].head + dgs[i].tail_ptr[v];
				        arc_i != arc_i_stop; ++arc_i) {
					if (row_markers[*arc_i] != v) {
						row_markers[*arc_i] = v;
						out_head[counter] = *arc_i;
						++counter;
					}
				}
			}
			out_tail_ptr[v + 1] = (scc_Arci) counter;
		}

	} else {
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			row_markers[v] = v;
			for (size_t i = 0; i < num_dgs; ++i) {
				const scc_Dpid* const arc_i_stop = dgs[i].head + dgs[i].tail_ptr[v + 1];
				for (const scc_Dpid* arc_i = dgs[i].head + dgs[i].tail_ptr[v];
				        arc_i != arc_i_stop; ++arc_i) {
					if (row_markers[*arc_i] != v) {
						row_markers[*arc_i] = v;
						++counter;
					}
				}
			}
		}
	}

	return counter;
}


static inline size_t iscc_do_union_and_delete(size_t vertices,
                                              size_t num_dgs,
                                              const iscc_Digraph dgs[restrict static num_dgs],
                                              scc_Dpid row_markers[restrict static vertices],
                                              const bool tails_to_keep[restrict static vertices],
                                              bool write,
                                              scc_Arci out_tail_ptr[restrict],
                                              scc_Dpid out_head[restrict])
{
	assert(vertices > 0);
	assert(num_dgs > 0);
	assert(dgs != NULL);
	assert(row_markers != NULL);
	assert(tails_to_keep != NULL);

	size_t counter = 0;
	if (write) out_tail_ptr[0] = 0;
	for (size_t v = 0; v < vertices; ++v) {
		row_markers[v] = SCC_DPID_NA;
	}

	assert(vertices < SCC_DPID_MAX);
	const scc_Dpid vertices_dpid = (scc_Dpid) vertices; // If `scc_Dpid` is signed 

	if (write) {
		assert(out_tail_ptr != NULL);
		assert(out_head != NULL);

		out_tail_ptr[0] = 0;
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			if (tails_to_keep[v]) {
				row_markers[v] = v;
				for (size_t i = 0; i < num_dgs; ++i) {
					const scc_Dpid* const arc_i_stop = dgs[i].head + dgs[i].tail_ptr[v + 1];
					for (const scc_Dpid* arc_i = dgs[i].head + dgs[i].tail_ptr[v];
					        arc_i != arc_i_stop; ++arc_i) {
						if (row_markers[*arc_i] != v) {
							row_markers[*arc_i] = v;
							out_head[counter] = *arc_i;
							++counter;
						}
					}
				}
			}
			out_tail_ptr[v + 1] = (scc_Arci) counter;
		}

	} else {
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			if (tails_to_keep[v]) {
				row_markers[v] = v;
				for (size_t i = 0; i < num_dgs; ++i) {
					const scc_Dpid* const arc_i_stop = dgs[i].head + dgs[i].tail_ptr[v + 1];
					for (const scc_Dpid* arc_i = dgs[i].head + dgs[i].tail_ptr[v];
					        arc_i != arc_i_stop; ++arc_i) {
						if (row_markers[*arc_i] != v) {
							row_markers[*arc_i] = v;
							++counter;
						}
					}
				}
			}
		}
	}

	return counter;
}


static inline size_t iscc_do_difference(const size_t vertices,
                                        const scc_Arci minuend_tail_ptr[const static vertices + 1],
                                        const scc_Dpid* const minuend_head,
                                        const scc_Arci subtrahend_tail_ptr[const static vertices + 1],
                                        const scc_Dpid* const subtrahend_head,
                                        const size_t max_out_degree,
                                        scc_Dpid row_markers[const restrict static vertices],
                                        const bool write,
                                        scc_Arci out_tail_ptr[const restrict],
                                        scc_Dpid out_head[const restrict])
{
	assert(vertices > 0);
	assert(minuend_tail_ptr != NULL);
	assert(minuend_head != NULL);
	assert(subtrahend_tail_ptr != NULL);
	assert(subtrahend_head != NULL);
	assert(max_out_degree > 0);
	assert(row_markers != NULL);

	size_t counter = 0;
	size_t row_counter;
	for (size_t v = 0; v < vertices; ++v) {
		row_markers[v] = SCC_DPID_NA;
	}

	assert(vertices < SCC_DPID_MAX);
	const scc_Dpid vertices_dpid = (scc_Dpid) vertices; // If `scc_Dpid` is signed

	if (write) {
		assert(out_tail_ptr != NULL);
		assert(out_head != NULL);

		out_tail_ptr[0] = 0;
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			row_markers[v] = v;
			const scc_Dpid* const v_arc_s_stop = subtrahend_head + subtrahend_tail_ptr[v + 1];
			for (const scc_Dpid* v_arc_s = subtrahend_head + subtrahend_tail_ptr[v];
			        v_arc_s != v_arc_s_stop; ++v_arc_s) {
				row_markers[*v_arc_s] = v;
			}

			row_counter = 0;
			const scc_Dpid* const arc_m_stop = minuend_head + minuend_tail_ptr[v + 1];
			for (const scc_Dpid* arc_m = minuend_head + minuend_tail_ptr[v];
			        ((row_counter < max_out_degree) && (arc_m != arc_m_stop)); ++arc_m) {
				if (row_markers[*arc_m] != v) {
					out_head[counter] = *arc_m;
					++row_counter;
					++counter;
				}
			}
			out_tail_ptr[v + 1] = (scc_Arci) counter;
		}

	} else {
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			row_markers[v] = v;
			const scc_Dpid* const v_arc_s_stop = subtrahend_head + subtrahend_tail_ptr[v + 1];
			for (const scc_Dpid* v_arc_s = subtrahend_head + subtrahend_tail_ptr[v];
			        v_arc_s != v_arc_s_stop; ++v_arc_s) {
				row_markers[*v_arc_s] = v;
			}

			row_counter = 0;
			const scc_Dpid* const arc_m_stop = minuend_head + minuend_tail_ptr[v + 1];
			for (const scc_Dpid* arc_m = minuend_head + minuend_tail_ptr[v];
			        ((row_counter < max_out_degree) && (arc_m != arc_m_stop)); ++arc_m) {
				if (row_markers[*arc_m] != v) {
					++row_counter;
					++counter;
				}
			}
		}
	}

	return counter;
}


static inline size_t iscc_do_adjacency_product(const size_t vertices,
                                               const scc_Arci dg_a_tail_ptr[const static vertices + 1],
                                               const scc_Dpid* const dg_a_head,
                                               const scc_Arci dg_b_tail_ptr[const static vertices + 1],
                                               const scc_Dpid* const dg_b_head,
                                               scc_Dpid row_markers[restrict const static vertices],
                                               const bool force_loops,
                                               const bool ignore_loops,
                                               const bool write,
                                               scc_Arci out_tail_ptr[restrict const],
                                               scc_Dpid out_head[restrict const])
{
	assert(vertices > 0);
	assert(dg_a_tail_ptr != NULL);
	assert(dg_a_head != NULL);
	assert(dg_b_tail_ptr != NULL);
	assert(dg_b_head != NULL);
	assert(row_markers != NULL);

	size_t counter = 0;
	for (size_t v = 0; v < vertices; ++v) {
		row_markers[v] = SCC_DPID_NA;
	}

	assert(vertices < SCC_DPID_MAX);
	const scc_Dpid vertices_dpid = (scc_Dpid) vertices; // If `scc_Dpid` is signed 

	if (write) {
		assert(out_tail_ptr != NULL);
		assert(out_head != NULL);

		out_tail_ptr[0] = 0;
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			row_markers[v] = v;
			if (force_loops) {
				const scc_Dpid* const v_arc_b_stop = dg_b_head + dg_b_tail_ptr[v + 1];
				for (const scc_Dpid* v_arc_b = dg_b_head + dg_b_tail_ptr[v];
				        v_arc_b != v_arc_b_stop; ++v_arc_b) {
					if (row_markers[*v_arc_b] != v) {
						row_markers[*v_arc_b] = v;
						out_head[counter] = *v_arc_b;
						++counter;
					}
				}
			}
			const scc_Dpid* const arc_a_stop = dg_a_head + dg_a_tail_ptr[v + 1];
			for (const scc_Dpid* arc_a = dg_a_head + dg_a_tail_ptr[v];
			        arc_a != arc_a_stop; ++arc_a) {
				if (*arc_a == v && (force_loops || ignore_loops)) continue;
				const scc_Dpid* const arc_b_stop = dg_b_head + dg_b_tail_ptr[*arc_a + 1];
				for (const scc_Dpid* arc_b = dg_b_head + dg_b_tail_ptr[*arc_a];
				        arc_b != arc_b_stop; ++arc_b) {
					if (row_markers[*arc_b] != v) {
						row_markers[*arc_b] = v;
						out_head[counter] = *arc_b;
						++counter;
					}
				}
			}
			out_tail_ptr[v + 1] = (scc_Arci) counter;
		}

	} else {
		for (scc_Dpid v = 0; v < vertices_dpid; ++v) {
			row_markers[v] = v;
			if (force_loops) {
				const scc_Dpid* const v_arc_b_stop = dg_b_head + dg_b_tail_ptr[v + 1];
				for (const scc_Dpid* v_arc_b = dg_b_head + dg_b_tail_ptr[v];
				        v_arc_b != v_arc_b_stop; ++v_arc_b) {
					if (row_markers[*v_arc_b] != v) {
						row_markers[*v_arc_b] = v;
						++counter;
					}
				}
			}
			const scc_Dpid* const arc_a_stop = dg_a_head + dg_a_tail_ptr[v + 1];
			for (const scc_Dpid* arc_a = dg_a_head + dg_a_tail_ptr[v];
			        arc_a != arc_a_stop; ++arc_a) {
				if (*arc_a == v && (force_loops || ignore_loops)) continue;
				const scc_Dpid* const arc_b_stop = dg_b_head + dg_b_tail_ptr[*arc_a + 1];
				for (const scc_Dpid* arc_b = dg_b_head + dg_b_tail_ptr[*arc_a];
				        arc_b != arc_b_stop; ++arc_b) {
					if (row_markers[*arc_b] != v) {
						row_markers[*arc_b] = v;
						++counter;
					}
				}
			}
		}
	}

	return counter;
}
