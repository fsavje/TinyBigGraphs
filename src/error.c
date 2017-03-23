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

#include "error.h"

#include <assert.h>
#include <stdio.h>
#include "../include/scclust.h"


// =============================================================================
// Static variables
// =============================================================================

static scc_ErrorCode iscc_error_code = SCC_ER_OK;
static const char* iscc_error_msg = NULL;
static const char* iscc_error_file = "unknown file";
static int iscc_error_line = -1;


// =============================================================================
// External function implementations
// =============================================================================

scc_ErrorCode iscc_make_error__(const scc_ErrorCode ec,
                                const char* const msg,
                                const char* const file,
                                const int line)
{
	assert((ec > SCC_ER_OK) && (ec <= SCC_ER_NOT_IMPLEMENTED));

	iscc_error_code = ec;
	iscc_error_msg = msg;
	iscc_error_file = file;
	iscc_error_line = line;

	return ec;
}


void iscc_reset_error(void)
{
	iscc_error_code = SCC_ER_OK;
	iscc_error_msg = NULL;
	iscc_error_file = "unknown file";
	iscc_error_line = -1;
}


bool scc_get_latest_error(const size_t len_error_message_buffer,
                          char error_message_buffer[const])
{
	if ((len_error_message_buffer == 0) || (error_message_buffer == NULL)) return false;

	if (iscc_error_code == SCC_ER_OK) {
		if (snprintf(error_message_buffer, len_error_message_buffer, "%s", "(scclust) No error.") < 0) {
			return false;
		}
		return true;
	}

	const char* error_message;
	if (iscc_error_msg != NULL) {
		error_message = iscc_error_msg;
	} else {
		switch (iscc_error_code) {
			case SCC_ER_UNKNOWN_ERROR:
				error_message = "Unkonwn error.";
				break;
			case SCC_ER_INVALID_INPUT:
				error_message = "Function parameters are invalid.";
				break;
			case SCC_ER_NO_MEMORY:
				error_message = "Cannot allocate required memory.";
				break;
			case SCC_ER_NO_SOLUTION:
				error_message = "Clustering problem has no solution.";
				break;
			case SCC_ER_TOO_LARGE_PROBLEM:
				error_message = "Clustering problem is too large.";
				break;
			case SCC_ER_DIST_SEARCH_ERROR:
				error_message = "Failed to calculate distances.";
				break;
			case SCC_ER_NOT_IMPLEMENTED:
				error_message = "Functionality not yet implemented.";
				break;
			default:
				error_message = "Unknown error code.";
				break;
		}
	}

	if (snprintf(error_message_buffer, len_error_message_buffer, "(scclust:%s:%d) %s", iscc_error_file, iscc_error_line, error_message) < 0) {
		return false;
	}

	return true;
}
