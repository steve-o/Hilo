/* Boney M. defined: round half up the river of Babylon.
 */

#ifndef __BNYMELLON_HH__
#define __BNYMELLON_HH__
#pragma once

#include <cmath>
#include <cstdint>

/* RFA 7.2 */
#include <rfa/rfa.hh>

namespace bnymellon
{

static inline
double
round_half_up (double x)
{
	return std::floor (x + 0.5);
}

#ifdef CONFIG_32BIT_PRICE

/* 32-bit: mantissa of 10E4, 4 decimal places
 */
static const int kMagnitude = rfa::data::ExponentNeg4;

static inline
int32_t
mantissa (double x)
{
	return (int32_t) round_half_up (x * 10000.0);
}

static inline
double
round (double x)
{
	return (double) mantissa (x) / 10000.0;
}

#else /* CONFIG_32BIT_PRICE */

/* 64-bit: mantissa of 10E6, 6 decimal places
 */
static const int kMagnitude = rfa::data::ExponentNeg6;

static inline
int64_t
mantissa (double x)
{
	return (int64_t) round_half_up (x * 1000000.0);
}

static inline
double
round (double x)
{
	return (double) mantissa (x) / 1000000.0;
}

#endif /* CONFIG_32BIT_PRICE */

} // namespace bnymellon

#endif /* __BNYMELLON_HH__ */

/* eof */
