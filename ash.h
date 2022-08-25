#include <stdint.h>

#ifndef ASH_H
#define ASH_H

int encode_data_frame(uint8_t *frame, int len, uint8_t *out_frame);
int decode_data_frame(uint8_t *frame, int len, uint8_t *out_frame);

#endif
