#ifndef MT19937AR_H
#define MT19937AR_H 1
#ifdef __cplusplus
extern "C" {
#endif
/* 
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)  
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.
   Copyright (C) 2005, Mutsuo Saito
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

#include <nds/jtypes.h>

#ifdef ARM7
#define ITCM_CODE
#endif

/* initializes mt[N] with a seed */
void ITCM_CODE init_genrand(unsigned long s);

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
//void init_by_array(unsigned long init_key[], int key_length);

/* generates a random number on [0,0xffffffff]-interval */
void ITCM_CODE genrand_regen();
unsigned long ITCM_CODE genrand_int32(void);
unsigned long ITCM_CODE genrand_int16(void);
unsigned long ITCM_CODE genrand_int8(void);
unsigned long ITCM_CODE genrand_int4(void);
unsigned long ITCM_CODE genrand_int2(void);

#define rand32() genrand_int32()
#define rand16() genrand_int16()
#define rand8() genrand_int8()
#define rand4() genrand_int4()

/* generates a random number on [0,0x7fffffff]-interval */
//long genrand_int31(void);

/* These real versions are due to Isaku Wada, 2002/01/09 added */
/* generates a random number on [0,1]-real-interval */
//double genrand_real1(void);

/* generates a random number on [0,1)-real-interval */
//double genrand_real2(void);

/* generates a random number on (0,1)-real-interval */
//double genrand_real3(void);

/* generates a random number on [0,1) with 53-bit resolution*/
//double genrand_res53(void);

// a convolution of a uniform distribution with itself is close to Gaussian
static inline unsigned long genrand_gaussian32() {
	return (u32)((genrand_int32()>>2) + (genrand_int32()>>2) +
			(genrand_int32()>>2) + (genrand_int32()>>2));
}

/* generates a random 28-bit integer in the range [0,m), 0 < m < 0x10000000 */
static inline unsigned long rand0(unsigned long n) {
	return (genrand_int32() >> 4) / (0x10000000/n);
}

/* returns true m times out of n, m <= n, n /= 0 */
static inline bool randMinN(unsigned long m, unsigned long n) {
	return (genrand_int32() < m*(0xffffffff/n));
}

#ifdef __cplusplus
}
#endif

#endif /* MT19937AR_H */
