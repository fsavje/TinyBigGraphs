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


/** @file
 *
 *  Public header for the scclust library.
 */


#ifndef SCC_SCCLUST_HG
#define SCC_SCCLUST_HG

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// ==============================================================================
// Compile options (user-serviceable)
// ==============================================================================

/* Uncomment to make clustering stable by sorting on vertex ID
 * This increases runtime considerably, generally only recommended for debugging.
 */
//#define SCC_STABLE_CLUSTERING


//#define SCC_EXTENSIVE_INPUT_CHECK


// ==============================================================================
// Library specific types, user-serviceable
// ==============================================================================

/** Type used for cluster labels. May be unsigned or signed.
 *  
 *  \note
 *  Possible cluster labels are the sequence `[0, 1, ..., SCC_CLABEL_MAX - 1]`. 
 *  `SCC_CLABEL_NA` may not be in this sequence.
 *  \note
 *  Number of clusters in any clustering problem must be strictly less
 *  than the maximum number that can be stored in #scc_Clabel (i.e., less
 *  than #SCC_CLABEL_MAX).
 */
typedef uint32_t scc_Clabel;

/// Maximum number that can be stored in #scc_Clabel.
static const scc_Clabel SCC_CLABEL_MAX = UINT32_MAX;

/// Label given to unassigned vertices.
static const scc_Clabel SCC_CLABEL_NA = UINT32_MAX;

/** Type used for data point IDs. May be unsigned or signed.
 *
 *  \note
 *  Possible data point IDs are the sequence `[0, 1, ..., SCC_DPID_MAX - 1]`. 
 *  `SCC_DPID_NA` may not be in this sequence.
 *  \note
 *  Number of data points in any clustering problem must be strictly less
 *  than the maximum number that can be stored in #scc_Dpid (i.e., less
 *  than #SCC_DPID_MAX).
 */
typedef uint32_t scc_Dpid;

/// Maximum number that can be stored in #scc_Dpid.
static const scc_Dpid SCC_DPID_MAX = UINT32_MAX;

/// Value to indicate that an invalid vertex.
static const scc_Dpid SCC_DPID_NA = UINT32_MAX;

/** Type used for arc indices. Must be unsigned.
 *  
 *  \note
 *  Number of arcs in any digraph must be less or equal to 
 *  the maximum number that can be stored in #scc_Arci.
 */
typedef uint32_t scc_Arci;

/// Maximum number that can be stored in #scc_Arci.
static const scc_Arci SCC_ARCI_MAX = UINT32_MAX;


// ==============================================================================
// Library specific types, non-serviceable
// ==============================================================================

/// Type used for data set objects. This struct is defined in `src/dist_search.c` (or by user). 
typedef struct scc_DataSetObject scc_DataSetObject;

/// Type used for clusterings
typedef struct scc_Clustering scc_Clustering;

/// Type used for clustering statistics
typedef struct scc_ClusteringStats scc_ClusteringStats;

/// Struct to store clustering statistics
struct scc_ClusteringStats {
	size_t num_populated_clusters;
	size_t num_assigned;
	size_t min_cluster_size;
	size_t max_cluster_size;
	double avg_cluster_size;
	double sum_dists;
	double min_dist;
	double max_dist;
	double cl_avg_min_dist;
	double cl_avg_max_dist;
	double cl_avg_dist_weighted;
	double cl_avg_dist_unweighted;
};


// ==============================================================================
// Error handling
// ==============================================================================


// ==============================================================================
// Utility functions
// ==============================================================================

/** Destructor for clusterings.
 *
 *  Frees the memory allocated by the input and writes the null clustering to it.
 *  Assumes the memory was allocated by the standard `malloc`, `calloc` or `realloc` functions.
 *
 *  \param[in,out] cl clustering to destroy. When #scc_free_Clustering returns, \p cl is set to #SCC_NULL_CLUSTERING.
 *
 *  \note If `cl->external_labels` is true, `cl->cluster_label` will *not* be freed by #scc_free_Clustering.
 */
void scc_free_clustering(scc_Clustering* cl);

scc_Clustering scc_init_empty_clustering(size_t num_data_points,
                                         scc_Clabel external_cluster_labels[]);

scc_Clustering scc_init_existing_clustering(size_t num_data_points,
                                            size_t num_clusters,
                                            scc_Clabel current_cluster_labels[],
                                            bool deep_label_copy);

bool scc_check_clustering(const scc_Clustering* cl,
                          bool extensive_check);

size_t scc_count_data_points(const scc_Clustering* cl);

size_t scc_count_clusters(const scc_Clustering* cl);

scc_Clabel* scc_get_labels(const scc_Clustering* cl);

void scc_make_labels_external(const scc_Clustering* cl);

scc_ClusteringStats scc_get_clustering_stats(const scc_Clustering* cl,
                                             scc_DataSetObject* data_set_object);


// ==============================================================================
// Greedy clustering function
// ==============================================================================

bool scc_top_down_greedy_clustering(scc_Clustering* cl,
                                    scc_DataSetObject* data_set_object,
                                    size_t size_constraint,
                                    bool batch_assign);

bool scc_bottom_up_greedy_clustering(scc_Clustering* cl,
                                     scc_DataSetObject* data_set_object,
                                     size_t size_constraint,
                                     bool batch_assign);


// ==============================================================================
// NNG-based clustering
// ==============================================================================


// ==============================================================================
// One-dimensional clustering
// ==============================================================================


#endif