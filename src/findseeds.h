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


#ifndef SCC_FINDSEEDS_HG
#define SCC_FINDSEEDS_HG

#include <stdbool.h>
#include <stddef.h>
#include "../include/digraph.h"
#include "../include/nng_clustering.h"


scc_SeedClustering iscc_findseeds_lexical(const scc_Digraph* nng,
                                          size_t seed_init_capacity);

scc_SeedClustering iscc_findseeds_inwards(const scc_Digraph* nng,
                                          size_t seed_init_capacity,
                                          bool updating);

scc_SeedClustering iscc_findseeds_exclusion(const scc_Digraph* nng,
                                            size_t seed_init_capacity,
                                            bool updating);



//iscc_findseeds_simulated_annealing();
//iscc_findseeds_approximation();


#endif
