/*
  Copyright (c) 2016 Arduino LLC. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**
 * @file
 * @defgroup thingy52_fft_analyzer_module FFT analyzer module
 * @{
 * @brief FFT analyzer module used in Thingy:52 demo.
 */

#ifndef FFT_ANALYZER_H__
#define FFT_ANALYZER_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/printk.h>

#define ARM_MATH_CM0PLUS
#include <arm_math.h>

#define FFT_NO_ERROR 0
#define FFT_ERROR 1

/**
 * @brief Check if the FFT analyzer has a result ready.
 *
 * @return  Returns true if FTT analyzer has a result ready.
 *          Returns false if FTT analyzer does not has a result ready.
 */
bool fft_analyzer_result_ready(void);

/**
 * @brief Get a processed spectrum.
 *
 * @param spectrum Pointer to the destination spectrum array.
 * @param size Size of the destination spectrum array.
 *
 * @retval FFT_NO_ERROR If successful.
 * @retval FFT_ERROR if error.
 */
int fft_analyzer_get_spectrum(int *spectrum, int size);

/**
 * @brief Configure the FFT analyzer.
 *
 * @param size Size of the PDM buffer.
 *
 * @retval FFT_NO_ERROR If successful.
 * @retval FFT_ERROR if error.
 */
int fft_analyzer_configure(int size);

/**
 * @brief Process an PDM buffer
 *
 * @param buffer PDM buffer.
 * @param size Size of PDM buffer.
 */
void fft_analyzer_process(const void *buffer, int size);

#endif /* FFT_ANALYZER_H__ */

/** @} */
