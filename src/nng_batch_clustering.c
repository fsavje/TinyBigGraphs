/* =============================================================================
 * scclust -- A C library for size constrained clustering
 * https://github.com/fsavje/scclust
 *
 * Copyright (C) 2015-2017  Fredrik Savje -- http://fredriksavje.com
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
 * ========================================================================== */

#include "nng_batch_clustering.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "../include/scclust.h"
#include "clustering_struct.h"
#include "dist_search.h"
#include "error.h"
#include "scclust_types.h"


// =============================================================================
// Static function prototypes
// =============================================================================

#ifdef SCC_STABLE_NNG

static int iscc_compare_PointIndex(const void* a, const void* b);

#endif // ifdef SCC_STABLE_NNG


static scc_ErrorCode iscc_run_nng_batches(scc_Clustering* clustering,
                                          iscc_NNSearchObject* nn_search_object,
                                          uint32_t size_constraint,
                                          bool ignore_unassigned,
                                          bool radius_constraint,
                                          double radius,
                                          const bool primary_data_points[],
                                          uint32_t batch_size,
                                          scc_PointIndex* batch_indices,
                                          scc_PointIndex* out_indices,
                                          bool* assigned);


// =============================================================================
// External function implementations
// =============================================================================

scc_ErrorCode scc_nng_clustering_batches(scc_Clustering* const clustering,
                                         void* const data_set,
                                         const uint32_t size_constraint,
                                         const scc_UnassignedMethod unassigned_method,
                                         const bool radius_constraint,
                                         const double radius,
                                         const size_t len_primary_data_points,
                                         const scc_PointIndex primary_data_points[const],
                                         uint32_t batch_size)
{
	if (!iscc_check_input_clustering(clustering)) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Invalid clustering object.");
	}
	if (!iscc_check_data_set(data_set, clustering->num_data_points)) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Invalid data set object.");
	}
	if (size_constraint < 2) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Size constraint must be 2 or greater.");
	}
	if (clustering->num_data_points < size_constraint) {
		return iscc_make_error_msg(SCC_ER_NO_SOLUTION, "Fewer data points than size constraint.");
	}
	if ((unassigned_method != SCC_UM_IGNORE) && (unassigned_method != SCC_UM_ANY_NEIGHBOR)) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Invalid unassigned method.");
	}
	if (radius_constraint && (radius <= 0.0)) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Invalid radius.");
	}
	if ((primary_data_points != NULL) && (len_primary_data_points == 0)) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Invalid primary data points.");
	}
	if ((primary_data_points == NULL) && (len_primary_data_points > 0)) {
		return iscc_make_error_msg(SCC_ER_INVALID_INPUT, "Invalid primary data points.");
	}
	if (clustering->num_clusters != 0) {
		return iscc_make_error_msg(SCC_ER_NOT_IMPLEMENTED, "Cannot refine existing clusterings.");
	}

	if (batch_size == 0) batch_size = UINT32_MAX;
	if (batch_size > clustering->num_data_points) {
		batch_size = (uint32_t) clustering->num_data_points;
	}

	iscc_NNSearchObject* nn_search_object;
	if (!iscc_init_nn_search_object(data_set,
	                                clustering->num_data_points,
	                                NULL,
	                                &nn_search_object)) {
		return iscc_make_error(SCC_ER_DIST_SEARCH_ERROR);
	}

	scc_PointIndex* const batch_indices = malloc(sizeof(scc_PointIndex[batch_size]));
	scc_PointIndex* const out_indices = malloc(sizeof(scc_PointIndex[size_constraint * batch_size]));
	bool* const assigned = calloc(clustering->num_data_points, sizeof(bool));
	if ((batch_indices == NULL) || (out_indices == NULL) || (assigned == NULL)) {
		free(batch_indices);
		free(out_indices);
		free(assigned);
		iscc_close_nn_search_object(&nn_search_object);
		return iscc_make_error(SCC_ER_NO_MEMORY);
	}

	// Initialize cluster labels
	if (clustering->cluster_label == NULL) {
		clustering->external_labels = false;
		clustering->cluster_label = malloc(sizeof(scc_Clabel[clustering->num_data_points]));
		if (clustering->cluster_label == NULL) {
			free(batch_indices);
			free(out_indices);
			free(assigned);
			iscc_close_nn_search_object(&nn_search_object);
			return iscc_make_error(SCC_ER_NO_MEMORY);
		}
	}

	bool* tmp_primary_data_points = NULL;
	if (primary_data_points != NULL) {
		tmp_primary_data_points = calloc(clustering->num_data_points, sizeof(bool));
		for (size_t i = 0; i < len_primary_data_points; ++i) {
			tmp_primary_data_points[primary_data_points[i]] = true;
		}
	}

	scc_ErrorCode ec = iscc_run_nng_batches(clustering,
	                                        nn_search_object,
	                                        size_constraint,
	                                        (unassigned_method == SCC_UM_IGNORE),
	                                        radius_constraint,
	                                        radius,
	                                        tmp_primary_data_points,
	                                        batch_size,
	                                        batch_indices,
	                                        out_indices,
	                                        assigned);

	free(batch_indices);
	free(out_indices);
	free(assigned);
	free(tmp_primary_data_points);
	iscc_close_nn_search_object(&nn_search_object);

	return ec;
}


// =============================================================================
// Static function implementations
// =============================================================================

#ifdef SCC_STABLE_NNG

static int iscc_compare_PointIndex(const void* const a, const void* const b)
{
    const scc_PointIndex arg1 = *(const scc_PointIndex* const)a;
    const scc_PointIndex arg2 = *(const scc_PointIndex* const)b;
    return (arg1 > arg2) - (arg1 < arg2);
}

#endif // ifdef SCC_STABLE_NNG


static scc_ErrorCode iscc_run_nng_batches(scc_Clustering* const clustering,
                                          iscc_NNSearchObject* const nn_search_object,
                                          const uint32_t size_constraint,
                                          const bool ignore_unassigned,
                                          const bool radius_constraint,
                                          const double radius,
                                          const bool primary_data_points[const],
                                          const uint32_t batch_size,
                                          scc_PointIndex* const batch_indices,
                                          scc_PointIndex* const out_indices,
                                          bool* const assigned)
{
	assert(iscc_check_input_clustering(clustering));
	assert(clustering->cluster_label != NULL);
	assert(clustering->num_clusters == 0);
	assert(nn_search_object != NULL);
	assert(size_constraint >= 2);
	assert(clustering->num_data_points >= size_constraint);
	assert(!radius_constraint || (radius > 0.0));
	assert(batch_size > 0);
	assert(batch_indices != NULL);
	assert(out_indices != NULL);
	assert(assigned != NULL);

	bool search_done = false;
	scc_Clabel next_cluster_label = 0;
	assert(clustering->num_data_points <= ISCC_POINTINDEX_MAX);
	const scc_PointIndex num_data_points = (scc_PointIndex) clustering->num_data_points; // If `scc_PointIndex` is signed

	for (scc_PointIndex curr_point = 0; curr_point < num_data_points; ) {

		size_t in_batch = 0;
		if (primary_data_points == NULL) {
			for (; (in_batch < batch_size) && (curr_point < num_data_points); ++curr_point) {
				if (!assigned[curr_point]) {
					clustering->cluster_label[curr_point] = SCC_CLABEL_NA;
					batch_indices[in_batch] = curr_point;
					++in_batch;
				}
			}
		} else {
			for (; (in_batch < batch_size) && (curr_point < num_data_points); ++curr_point) {
				if (!assigned[curr_point]) {
					clustering->cluster_label[curr_point] = SCC_CLABEL_NA;
					if (primary_data_points[curr_point]) {
						batch_indices[in_batch] = curr_point;
						++in_batch;
					}
				}
			}
		}

		if (in_batch == 0) {
			assert(curr_point == num_data_points);
			break;
		}

		size_t num_ok_in_batch = 0;
		search_done = true;
		if (!iscc_nearest_neighbor_search(nn_search_object,
		                                  in_batch,
		                                  batch_indices,
		                                  size_constraint,
		                                  radius_constraint,
		                                  radius,
		                                  &num_ok_in_batch,
		                                  batch_indices,
		                                  out_indices)) {
			return iscc_make_error(SCC_ER_DIST_SEARCH_ERROR);
		}

		#ifdef SCC_STABLE_NNG
		for (size_t i = 0; i < num_ok_in_batch; ++i) {
			qsort(out_indices + i * size_constraint, size_constraint, sizeof(scc_PointIndex), iscc_compare_PointIndex);
		}
		#endif // ifdef SCC_STABLE_NNG

		const scc_PointIndex* check_indices = out_indices;
		for (size_t i = 0; i < num_ok_in_batch; ++i) {
			const scc_PointIndex* const stop_check_indices = check_indices + size_constraint;
			if (!assigned[batch_indices[i]]) {
				for (; (check_indices != stop_check_indices) && !assigned[*check_indices]; ++check_indices) {}
				if (check_indices == stop_check_indices) {
					// `i` has no assigned neighbors and can be seed
					if (next_cluster_label == SCC_CLABEL_MAX) {
						return iscc_make_error_msg(SCC_ER_TOO_LARGE_PROBLEM, "Too many clusters (adjust the `scc_Clabel` type).");
					}

					assert(!assigned[batch_indices[i]]);
					const scc_PointIndex* const stop_assign_indices = stop_check_indices - 1;
					for (check_indices -= size_constraint; check_indices != stop_assign_indices; ++check_indices) {
						assert(!assigned[*check_indices]);
						assigned[*check_indices] = true;
						clustering->cluster_label[*check_indices] = next_cluster_label;
					}
					if (assigned[batch_indices[i]]) {
						// Self-loop from `batch_indices[i]` to `batch_indices[i]` existed among NN
						assert(!assigned[*check_indices]);
						assigned[*check_indices] = true;
						clustering->cluster_label[*check_indices] = next_cluster_label;
					} else {
						// Self-loop did not exist
						assert(!assigned[batch_indices[i]]);
						assigned[batch_indices[i]] = true;
						clustering->cluster_label[batch_indices[i]] = next_cluster_label;
					}

					assert(clustering->cluster_label[batch_indices[i]] == next_cluster_label);
					++next_cluster_label;
				} else {
					// `i` has assigned neighbors and cannot be seed
					if (!ignore_unassigned) {
						// Assign `batch_indices[i]` to a preliminary cluster.
						// If a future seed wants it as neighbor, it switches cluster.
						assert(assigned[*check_indices]);
						assert(clustering->cluster_label[batch_indices[i]] == SCC_CLABEL_NA);
						assert(clustering->cluster_label[*check_indices] != SCC_CLABEL_NA);
						assert(!assigned[batch_indices[i]]);
						clustering->cluster_label[batch_indices[i]] = clustering->cluster_label[*check_indices];
					}
				}
			}
			check_indices = stop_check_indices;
		} // Loop in batch
	} // Loop between batches

	if (next_cluster_label == 0) {
		if (!search_done) {
			// Never did search, i.e., primary_data_points are all false
			assert(primary_data_points != NULL);
			return iscc_make_error_msg(SCC_ER_NO_SOLUTION, "No primary data points.");
		} else {
			// Did search but still no clusters, i.e., too tight radius constraint
			assert(radius_constraint);
			return iscc_make_error_msg(SCC_ER_NO_SOLUTION, "Infeasible radius constraint.");
		}
	}

	clustering->num_clusters = (size_t) next_cluster_label;

	return iscc_no_error();
}
