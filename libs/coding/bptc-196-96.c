/*
 * This file is part of dmrshark.
 *
 * dmrshark is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dmrshark is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dmrshark.  If not, see <http://www.gnu.org/licenses/>.
**/

#include DEFAULTCONFIG

#include "bptc-196-96.h"

#include <libs/daemon/console.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
	flag_t bits[4];
} bptc_196_96_error_vector_t;

// Hamming(15, 11, 3) checking of a matrix row (15 total bits, 11 data bits, min. distance: 3)
// See page 135 of the DMR Air Interface protocol specification for the generator matrix.
// A generator matrix looks like this: G = [Ik | P]. The parity check matrix is: H = [-P^T|In-k]
// In binary codes, then -P = P, so the negation is unnecessary. We can get the parity check matrix
// only by transposing the generator matrix. We then take a data row, and multiply it with each row
// of the parity check matrix, then xor each resulting row bits together with the corresponding
// parity check bit. The xor result (error vector) should be 0, if it's not, it can be used
// to determine the location of the erroneous bit using the generator matrix (P).
static void bptc_196_96_hamming_15_11_3_get_parity_bits(flag_t *data_bits, bptc_196_96_error_vector_t *error_vector) {
	if (data_bits == NULL || error_vector == NULL)
		return;

	error_vector->bits[0] = (data_bits[0] ^ data_bits[1] ^ data_bits[2] ^ data_bits[3] ^ data_bits[5] ^ data_bits[7] ^ data_bits[8]);
	error_vector->bits[1] = (data_bits[1] ^ data_bits[2] ^ data_bits[3] ^ data_bits[4] ^ data_bits[6] ^ data_bits[8] ^ data_bits[9]);
	error_vector->bits[2] = (data_bits[2] ^ data_bits[3] ^ data_bits[4] ^ data_bits[5] ^ data_bits[7] ^ data_bits[9] ^ data_bits[10]);
	error_vector->bits[3] = (data_bits[0] ^ data_bits[1] ^ data_bits[2] ^ data_bits[4] ^ data_bits[6] ^ data_bits[7] ^ data_bits[10]);
}

static flag_t bptc_196_96_hamming_15_11_3_errorcheck(flag_t *data_bits, bptc_196_96_error_vector_t *error_vector) {
	if (data_bits == NULL || error_vector == NULL)
		return 0;

	bptc_196_96_hamming_15_11_3_get_parity_bits(data_bits, error_vector);

	error_vector->bits[0] ^= data_bits[11];
	error_vector->bits[1] ^= data_bits[12];
	error_vector->bits[2] ^= data_bits[13];
	error_vector->bits[3] ^= data_bits[14];

	if (error_vector->bits[0] == 0 &&
		error_vector->bits[1] == 0 &&
		error_vector->bits[2] == 0 &&
		error_vector->bits[3] == 0)
			return 1;

	console_log(LOGLEVEL_CODING LOGLEVEL_DEBUG "    bptc (196,96): hamming(15,11) error vector: %u%u%u%u\n",
		error_vector->bits[0],
		error_vector->bits[1],
		error_vector->bits[2],
		error_vector->bits[3]);

	return 0;
}

static void bptc_196_96_hamming_13_9_3_get_parity_bits(flag_t *data_bits, bptc_196_96_error_vector_t *error_vector) {
	if (data_bits == NULL || error_vector == NULL)
		return;

	error_vector->bits[0] = (data_bits[0] ^ data_bits[1] ^ data_bits[3] ^ data_bits[5] ^ data_bits[6]);
	error_vector->bits[1] = (data_bits[0] ^ data_bits[1] ^ data_bits[2] ^ data_bits[4] ^ data_bits[6] ^ data_bits[7]);
	error_vector->bits[2] = (data_bits[0] ^ data_bits[1] ^ data_bits[2] ^ data_bits[3] ^ data_bits[5] ^ data_bits[7] ^ data_bits[8]);
	error_vector->bits[3] = (data_bits[0] ^ data_bits[2] ^ data_bits[4] ^ data_bits[5] ^ data_bits[8]);
}

// Hamming(13, 9, 3) checking of a matrix column (13 total bits, 9 data bits, min. distance: 3)
static flag_t bptc_196_96_hamming_13_9_3_errorcheck(flag_t *data_bits, bptc_196_96_error_vector_t *error_vector) {
	if (data_bits == NULL || error_vector == NULL)
		return 0;

	bptc_196_96_hamming_13_9_3_get_parity_bits(data_bits, error_vector);

	error_vector->bits[0] ^= data_bits[9];
	error_vector->bits[1] ^= data_bits[10];
	error_vector->bits[2] ^= data_bits[11];
	error_vector->bits[3] ^= data_bits[12];

	if (error_vector->bits[0] == 0 &&
		error_vector->bits[1] == 0 &&
		error_vector->bits[2] == 0 &&
		error_vector->bits[3] == 0)
			return 1;

	console_log(LOGLEVEL_CODING LOGLEVEL_DEBUG "    bptc (196,96): hamming(13,9) error vector: %u%u%u%u\n",
		error_vector->bits[0],
		error_vector->bits[1],
		error_vector->bits[2],
		error_vector->bits[3]);

	return 0;
}

static void bptc_196_96_display_data_matrix(flag_t deinterleaved_bits[196]) {
	loglevel_t loglevel = console_get_loglevel();
	uint8_t row, col;

	if (!loglevel.flags.debug && !loglevel.flags.coding)
		return;

	console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING "    bptc (196,96) matrix:\n");
	for (row = 0; row < 13; row++) {
		console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING "      #%.2u ", row);
		for (col = 0; col < 11; col++) {
			// +1 because the first bit is R(3) and it's not used so we can ignore that.
			console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING "%u", deinterleaved_bits[col+row*15+1]);
		}
		console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING " ");
		for (; col < 15; col++) {
			// +1 because the first bit is R(3) and it's not used so we can ignore that.
			console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING "%u", deinterleaved_bits[col+row*15+1]);
		}
		console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING "\n");
		if (row == 8)
			console_log(LOGLEVEL_DEBUG LOGLEVEL_CODING "\n");
	}
}

// Searches for the given error vector in the generator matrix.
// Returns the erroneous bit number if the error vector is found, otherwise it returns -1.
static int bptc_196_96_find_hamming_15_11_3_error_position(bptc_196_96_error_vector_t *error_vector) {
	static flag_t hamming_15_11_generator_matrix[] = {
		1, 0, 0, 1,
		1, 1, 0, 1,
		1, 1, 1, 1,
		1, 1, 1, 0,
		0, 1, 1, 1,
		1, 0, 1, 0,
		0, 1, 0, 1,
		1, 0, 1, 1,
		1, 1, 0, 0,
		0, 1, 1, 0,
		0, 0, 1, 1,

		1, 0, 0, 0, // These are used to determine errors in the Hamming checksum bits.
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	uint8_t row;

	for (row = 0; row < 15; row++) {
		if (hamming_15_11_generator_matrix[row*4] == error_vector->bits[0] &&
			hamming_15_11_generator_matrix[row*4+1] == error_vector->bits[1] &&
			hamming_15_11_generator_matrix[row*4+2] == error_vector->bits[2] &&
			hamming_15_11_generator_matrix[row*4+3] == error_vector->bits[3])
				return row;
	}

	return -1;
}

// Searches for the given error vector in the generator matrix.
// Returns the erroneous bit number if the error vector is found, otherwise it returns -1.
static int bptc_196_96_find_hamming_13_9_3_error_position(bptc_196_96_error_vector_t *error_vector) {
	static flag_t hamming_13_9_generator_matrix[] = {
		1, 1, 1, 1,
		1, 1, 1, 0,
		0, 1, 1, 1,
		0, 1, 1, 1,
		0, 1, 0, 1,
		1, 0, 1, 1,
		1, 1, 0, 0,
		0, 1, 1, 0,
		0, 0, 1, 1,

		1, 0, 0, 0, // These are used to determine errors in the Hamming checksum bits.
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	uint8_t row;

	if (error_vector == NULL)
		return -1;

	for (row = 0; row < 13; row++) {
		if (hamming_13_9_generator_matrix[row*4] == error_vector->bits[0] &&
			hamming_13_9_generator_matrix[row*4+1] == error_vector->bits[1] &&
			hamming_13_9_generator_matrix[row*4+2] == error_vector->bits[2] &&
			hamming_13_9_generator_matrix[row*4+3] == error_vector->bits[3])
				return row;
	}

	return -1;
}

// Checks data for errors and tries to repair them.
flag_t bptc_196_96_check_and_repair(flag_t deinterleaved_bits[196]) {
	bptc_196_96_error_vector_t bptc_196_96_error_vector;
	flag_t column_bits[13] = {0,};
	uint8_t row, col;
	int8_t wrongbitnr = -1;
	flag_t errors_found = 0;
	flag_t result = 1;

	if (deinterleaved_bits == NULL)
		return 0;

	bptc_196_96_display_data_matrix(deinterleaved_bits);

	for (col = 0; col < 15; col++) {
		for (row = 0; row < 13; row++) {
			// +1 because the first bit is R(3) and it's not used so we can ignore that.
			column_bits[row] = deinterleaved_bits[col+row*15+1];
		}

		if (!bptc_196_96_hamming_13_9_3_errorcheck(column_bits, &bptc_196_96_error_vector)) {
			errors_found = 1;
			// Error check failed, checking if we can determine the location of the bit error.
			wrongbitnr = bptc_196_96_find_hamming_13_9_3_error_position(&bptc_196_96_error_vector);
			if (wrongbitnr < 0) {
				result = 0;
				console_log(LOGLEVEL_CODING "    bptc (196,96): hamming(13,9) check error, can't repair column #%u\n", col);
			} else {
				// +1 because the first bit is R(3) and it's not used so we can ignore that.
				console_log(LOGLEVEL_CODING "    bptc (196,96): hamming(13,9) check error, fixing bit row #%u col #%u\n", wrongbitnr, col);
				deinterleaved_bits[col+wrongbitnr*15+1] = !deinterleaved_bits[col+wrongbitnr*15+1];

				bptc_196_96_display_data_matrix(deinterleaved_bits);

				for (row = 0; row < 13; row++) {
					// +1 because the first bit is R(3) and it's not used so we can ignore that.
					column_bits[row] = deinterleaved_bits[col+row*15+1];
				}

				if (!bptc_196_96_hamming_13_9_3_errorcheck(column_bits, &bptc_196_96_error_vector)) {
					result = 0;
					console_log(LOGLEVEL_CODING "    bptc (196,96): hamming(13,9) check error, couldn't repair column #%u\n", col);
				}
			}
		}
	}

	for (row = 0; row < 9; row++) {
		// +1 because the first bit is R(3) and it's not used so we can ignore that.
		if (!bptc_196_96_hamming_15_11_3_errorcheck(&deinterleaved_bits[row*15+1], &bptc_196_96_error_vector)) {
			errors_found = 1;
			// Error check failed, checking if we can determine the location of the bit error.
			wrongbitnr = bptc_196_96_find_hamming_15_11_3_error_position(&bptc_196_96_error_vector);
			if (wrongbitnr < 0) {
				result = 0;
				console_log(LOGLEVEL_CODING "    bptc (196,96): hamming(15,11) check error in row %u, can't repair\n", row);
			} else {
				// +1 because the first bit is R(3) and it's not used so we can ignore that.
				console_log(LOGLEVEL_CODING "    bptc (196,96): hamming(15,11) check error, fixing bit row #%u col #%u\n", row, wrongbitnr);
				deinterleaved_bits[row*15+wrongbitnr+1] = !deinterleaved_bits[row*15+wrongbitnr+1];

				bptc_196_96_display_data_matrix(deinterleaved_bits);

				if (!bptc_196_96_hamming_15_11_3_errorcheck(&deinterleaved_bits[row*15+1], &bptc_196_96_error_vector)) {
					result = 0;
					console_log(LOGLEVEL_CODING "    bptc (196,96): hamming(15,11) check error, couldn't repair row #%u\n", row);
				}
			}
		}
	}

	if (result && !errors_found)
		console_log(LOGLEVEL_CODING "    bptc (196,96): received data was error free\n");
	else if (result && errors_found)
		console_log(LOGLEVEL_CODING "    bptc (196,96): received data had errors which were corrected\n");
	else if (!result)
		console_log(LOGLEVEL_CODING "    bptc (196,96): received data had errors which couldn't be corrected\n");

	return result;
}

// Extracts the data bits from the given deinterleaved info bits array (discards BPTC bits).
bptc_196_96_data_bits_t *bptc_196_96_extractdata(flag_t deinterleaved_bits[196]) {
	static bptc_196_96_data_bits_t data_bits;

	if (deinterleaved_bits == NULL)
		return NULL;

	memcpy(&data_bits.bits[0], &deinterleaved_bits[4], 8);
	memcpy(&data_bits.bits[8], &deinterleaved_bits[16], 11);
	memcpy(&data_bits.bits[19], &deinterleaved_bits[31], 11);
	memcpy(&data_bits.bits[30], &deinterleaved_bits[46], 11);
	memcpy(&data_bits.bits[41], &deinterleaved_bits[61], 11);
	memcpy(&data_bits.bits[52], &deinterleaved_bits[76], 11);
	memcpy(&data_bits.bits[63], &deinterleaved_bits[91], 11);
	memcpy(&data_bits.bits[74], &deinterleaved_bits[106], 11);
	memcpy(&data_bits.bits[85], &deinterleaved_bits[121], 11);

	return &data_bits;
}

// Generates 196 BPTC payload info bits from 96 data bits.
dmrpacket_payload_info_bits_t *bptc_196_96_generate(bptc_196_96_data_bits_t *data_bits) {
	static dmrpacket_payload_info_bits_t payload_info_bits;
	bptc_196_96_error_vector_t error_vector;
	uint8_t col, row;
	uint8_t dbp;
	flag_t column_bits[9] = {0,};

	memset(payload_info_bits.bits, 0, sizeof(dmrpacket_payload_info_bits_t));

	dbp = 0;
	for (row = 0; row < 9; row++) {
		if (row == 0) {
			for (col = 3; col < 11; col++) {
				// +1 because the first bit is R(3) and it's not used so we can ignore that.
				payload_info_bits.bits[col+1] = data_bits->bits[dbp++];
			}
		} else {
			for (col = 0; col < 11; col++) {
				// +1 because the first bit is R(3) and it's not used so we can ignore that.
				payload_info_bits.bits[col+row*15+1] = data_bits->bits[dbp++];
			}
		}

		// +1 because the first bit is R(3) and it's not used so we can ignore that.
		bptc_196_96_hamming_15_11_3_get_parity_bits(&payload_info_bits.bits[row*15+1], &error_vector);
		payload_info_bits.bits[row*15+11+1] = error_vector.bits[0];
		payload_info_bits.bits[row*15+12+1] = error_vector.bits[1];
		payload_info_bits.bits[row*15+13+1] = error_vector.bits[2];
		payload_info_bits.bits[row*15+14+1] = error_vector.bits[3];
	}

	for (col = 0; col < 15; col++) {
		for (row = 0; row < 9; row++)
			column_bits[row] = payload_info_bits.bits[col+row*15+1];

		bptc_196_96_hamming_13_9_3_get_parity_bits(column_bits, &error_vector);
		payload_info_bits.bits[col+135+1] = error_vector.bits[0];
		payload_info_bits.bits[col+135+15+1] = error_vector.bits[1];
		payload_info_bits.bits[col+135+30+1] = error_vector.bits[2];
		payload_info_bits.bits[col+135+45+1] = error_vector.bits[3];
	}

	//console_log(LOGLEVEL_CODING LOGLEVEL_DEBUG "bptc (196,96): constructed matrix:\n");
	//bptc_196_96_display_data_matrix(payload_info_bits.bits);

	return &payload_info_bits;
}
