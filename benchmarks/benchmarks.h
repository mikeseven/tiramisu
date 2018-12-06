/*
 * benchmarks.h
 *
 *  Created on: Oct 13, 2016
 *      Author: b
 */

#ifndef BENCHMARKS_BENCHMARKS_H_
#define BENCHMARKS_BENCHMARKS_H_


#define NB_TESTS 60
#define CHECK_CORRECTNESS 1
#define PRINT_OUTPUT 0 
#define SIZE_IS_MULTIPLE_OF_TILE 1


// BARYON_N is used for loop iterators.
#define BARYON_N 16
#define BX 16
#define BY 16
#define BZ 16
#define BT 16
// BARYON_P1 is used for the size of first dimension
// of array and possible value of the parameters used
// in that first dimension.
#define BARYON_P1 3
// BARYON_P is used for the size of the other array
// dimensions that are not of size BARYON_N
#define BARYON_P 1

// Data size
#if TIRAMISU_XLARGE

const size_t N=4096;
const size_t M=N;
const size_t K=N;
#define SIZE (65536*4096)

#elif TIRAMISU_LARGE

#if SIZE_IS_MULTIPLE_OF_TILE
const size_t N=1024;
const size_t M=1024;
const size_t K=1024;
#define SIZE (1024*1024)
#else
const size_t N=1060;
const size_t M=1060;
const size_t K=1060;
#endif

#elif TIRAMISU_MEDIUM

const size_t N=512;
const size_t M=512;
const size_t K=512;
#define SIZE (1024)

#elif TIRAMISU_SMALL

const size_t N=128;
const size_t M=128;
const size_t K=128;
#define SIZE (128)

#else

const size_t N=1024;
const size_t M=1024;
const size_t K=1024;

#endif

#endif /* BENCHMARKS_BENCHMARKS_H_ */
