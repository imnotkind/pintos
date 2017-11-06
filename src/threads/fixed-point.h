#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#define POINT_POS 14 // 17.14 format. 1 bit is signed bit
#define CORR (1 << POINT_POS)
#include <inttypes.h>
typedef int32_t Fixed;

Fixed fixed_add(Fixed a, Fixed b);

Fixed fixed_sub(Fixed a, Fixed b);

Fixed fixed_mul(Fixed a, Fixed b);

Fixed fixed_div(Fixed a, Fixed b);

Fixed itofixed(int a);

int fixedtoi(Fixed a);

#endif