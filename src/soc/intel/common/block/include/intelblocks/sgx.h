/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SOC_INTEL_COMMON_BLOCK_SGX_H
#define SOC_INTEL_COMMON_BLOCK_SGX_H

/*
 * Lock SGX memory.
 * CPU specific code needs to provide the implementation.
 */
void cpu_lock_sgx_memory(void);

/*
 * Configure core PRMRR.
 * PRMRR needs to configured first on all cores and then
 * call sgx_configure() for all cores to init SGX.
 */
void prmrr_core_configure(void);

/*
 * Configure SGX.
 */
void sgx_configure(void);

#endif	/* SOC_INTEL_COMMON_BLOCK_SGX_H */
