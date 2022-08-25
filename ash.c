#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ash.h"

static const uint8_t flag = 0x7e;
static const uint8_t escape = 0x7d;
static const uint8_t xon = 0x11;
static const uint8_t xoff = 0x12;
static const uint8_t substitude = 0x18;
static const uint8_t cancel = 0x1a;

static const uint8_t stuff = 0x20;

static const uint8_t randomize_start = 0x42;
static const uint8_t randomize_seq = 0xb8;

static uint8_t frm_seq = 0;
static uint8_t rec_seq = 0;


static void xor_data_frame_payload(uint8_t *payload, int len)
{
	int i;
	uint8_t rand = randomize_start;

	for (i = 0; i < len; i++)
	{
		payload[i] = payload[i] ^ rand;
		if (rand % 2)
			rand = (rand >> 1) ^ randomize_seq;
		else
			rand = rand >> 1;
	}
}

/*
 * Stuff frame for ASH reserved bytes
 *
 * target buffer must be twice the size of len!
 */
int stuff_frame(const uint8_t *frame, int len, uint8_t *target)
{
	int i, dst;

	for (i = 0, dst = 0; i < len; i++) {
		if (frame[i] == flag || frame[i] == escape ||
		    frame[i] == xon || frame[i] == xoff ||
		    frame[i] == substitude || frame[i] == cancel) {
			target[dst++] = escape;
			target[dst++] = frame[i] ^ stuff;
		} else {
			target[dst++] = frame[i];
		}
	}

	return dst;
}

int unstuff_frame(const uint8_t *frame, int len, uint8_t *target)
{
	int i, dst;

	for (i = 0, dst = 0; i < len; i++) {
		if (frame[i] == escape) {
			i++;
			target[dst++] = frame[i] ^ stuff;
		} else {
			target[dst++] = frame[i];
		}
	}

	return dst;
}

uint16_t crc16(const uint8_t* frame, int len){
	uint8_t x;
	uint16_t crc = 0xffff;

	while (len--) {
		x = crc >> 8 ^ *frame++;
		x ^= x>>4;
		crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
	}
	return crc;
}

int encode_data_frame(uint8_t *frame, int len, uint8_t *out_frame)
{
	int out_len;
	uint8_t retrans = 0;
	uint16_t crc;
	uint8_t *tmp;

	xor_data_frame_payload(frame, len);
       
	tmp = malloc(len * 2 + 3);
	tmp[0] = ((frm_seq & 0x7) << 4) | (retrans << 3) | (rec_seq & 0x7);
	frm_seq++;
	memcpy(tmp + 1, frame, len);
	crc = crc16(tmp, len + 1);
	tmp[len + 1] = (uint8_t)(crc >> 8);
	tmp[len + 2] = (uint8_t)(crc & 0xff);
	out_len = stuff_frame(tmp, len + 3, out_frame);
	out_frame[out_len] = flag;

	/* Account for control + CRC */
	return out_len + 1;
}

int decode_data_frame(uint8_t *frame, int len, uint8_t *out_frame)
{
	uint16_t crc;
	uint8_t *tmp;

	/* Strip flag */
	len--;
	if (frame[len] != flag)
		fprintf(stderr, "ERROR, no flag at end!\n");

	tmp = malloc(len);
	unstuff_frame(frame, len, tmp);
	len -= 2;
	crc = crc16(tmp, len);
	if (tmp[len + 0] != (uint8_t)(crc >> 8) || 
	    tmp[len + 1] != (uint8_t)(crc & 0xff)) {
		fprintf(stderr, "CRC error!\n");
		return -1;
	}

	/* Just ignore non-data frames for now... */
	if (tmp[0] & 0x80)
		return 0;

	rec_seq++;

	/* Strip control frame */
	len--;
	memcpy(out_frame, tmp + 1, len);
	xor_data_frame_payload(out_frame, len);

	return len;
}
