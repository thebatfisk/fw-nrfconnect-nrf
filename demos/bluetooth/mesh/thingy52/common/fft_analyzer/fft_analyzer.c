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

#include "fft_analyzer.h"

static int _size;
static bool _result_ready;
static int *_sample_buffer;
static int _sample_buffer_size;
static int *_fft_buffer;
static int *_spectrum_buffer;

static arm_rfft_instance_q15 _S15;

bool fft_analyzer_result_ready(void)
{
	return _result_ready;
}

int fft_analyzer_get_spectrum(int *spectrum, int size)
{
	if (!_result_ready) {
		return FFT_ERROR;
	}

	if (size > (_size / 2)) {
		size = _size / 2;
	}

	q31_t *dst = (q31_t *)spectrum;
	q15_t *src = (q15_t *)_spectrum_buffer;

	for (int i = 0; i < size; i++) {
		*dst++ = *src++;
	}

	_result_ready = false;

	return size;
}

int fft_analyzer_configure(int size)
{
	_size = size;
	_sample_buffer_size = _size * sizeof(q15_t);

	if (ARM_MATH_SUCCESS != arm_rfft_init_q15(&_S15, _size, 0, 1)) {
		return FFT_ERROR;
	}

	_sample_buffer = (int *)calloc(_sample_buffer_size, 1);
	_spectrum_buffer = (int *)calloc(_size, sizeof(q15_t));

	_fft_buffer = (int *)calloc(_sample_buffer_size * 2, 1);

	if (_sample_buffer == NULL || _spectrum_buffer == NULL ||
	    _fft_buffer == NULL) {
		return FFT_ERROR;
	}

	return FFT_NO_ERROR;
}

/* This function takes 0.5536 ms */
void fft_analyzer_process(const void *buffer, int size)
{
	if (size > _sample_buffer_size) {
		/* More samples than buffer size, cap */
		size = _sample_buffer_size;
	}

	int newSamplesOffset = _sample_buffer_size - size;

	if (newSamplesOffset) {
		/* Shift over the previous samples */
		memmove(_sample_buffer, ((uint8_t *)_sample_buffer) + size,
			newSamplesOffset);
	}

	uint8_t *newSamples = ((uint8_t *)_sample_buffer) + newSamplesOffset;

	memcpy(newSamples, buffer, size);

	arm_rfft_q15(&_S15, (q15_t *)_sample_buffer, (q15_t *)_fft_buffer);
	arm_cmplx_mag_q15((q15_t *)_fft_buffer, (q15_t *)_spectrum_buffer,
			  _size);

	_result_ready = true;
}
