#include <stdint.h>

#ifndef ASH_H
#define ASH_H

void ash_init(void);
int ash_encode_data_frame(uint8_t *frame, int len, uint8_t *out_frame);
int ash_decode_data(uint8_t data, uint8_t *out_frame);

#endif
