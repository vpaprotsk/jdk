/*
* Copyright (c) 2022, Intel Corporation. All rights reserved.
*
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* This code is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 only, as
* published by the Free Software Foundation.
*
* This code is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
* version 2 for more details (a copy is included in the LICENSE file that
* accompanied this code).
*
* You should have received a copy of the GNU General Public License version
* 2 along with this work; if not, write to the Free Software Foundation,
* Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
*
* Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
* or visit www.oracle.com if you need additional information or have any
* questions.
*
*/

#include "precompiled.hpp"
#include "asm/assembler.hpp"
#include "asm/assembler.inline.hpp"
#include "runtime/stubRoutines.hpp"
#include "macroAssembler_x86.hpp"

#ifdef _LP64

enum polyCPOffset {
  high_bit = 0,
  mask_44 = 64,
  mask_42 = 128,
};

// Compute product for 8 16-byte message blocks,
//
//      a2       a1       a0
// ×    r2       r1       r0
// ----------------------------------
//     a2×r0    a1×r0    a0×r0
// +   a1×r1    a0×r1  5×a2×r1'     (r1' = r1<<2)
// +   a0×r2  5×a2×r2' 5×a1×r2'     (r2' = r2<<2)
// ----------------------------------
//        p2       p1       p0
//
// Then, propagate the carry (bits after bit 43) from lower limbs into higher limbs.
// This requires two passes to guarantee no further carries
//
// Explanation for some 'well known' modular arithmetic optimizations:
// Math Note 1: Reduction by 2^130-5 can be expressed as follows:
//    ( a×2^130 + b ) mod 2^130-5 
//                                 = ( a×2^130 - 5×a + 5×a + b ) mod 2^130-5 
//                                 = ( a×(2^130 - 5) + 5×a + b ) mod 2^130-5 // i.e. adding multiples of modulus is a noop
//                                 = ( 5×a + b ) mod 2^130-5
// proves the well known algorithm of 'split the number down the middle, multiply upper and add'
// See "Cheating at modular arithmetic" and "Poly1305's prime: 2^130 - 5"
//      paragraphs at https://loup-vaillant.fr/tutorials/poly1305-design for more details.
//
// Math Note 2: 'Propagation' from p2 to p0 involves multiplication by 5 since we are working on modular arithmetic:
//    ( p2×2^88 ) mod 2^130-5
//                             = ( p2'×2^88 + p2''×2^130) mod 2^130-5 // Split on 130-bit boudary
//                             = ( p2'×2^88 + p2''×2^130 - 5×p2'' + 5×p2'') mod 2^130-5
//                             = ( p2'×2^88 + p2''×(2^130 - 5) + 5×p2'') mod 2^130-5 // i.e. adding multiples of modulus is a noop
//                             = ( p2'×2^88 + 5×p2'') mod 2^130-5
// 
// Math Note 2: R1P = 4*5*R1 and R2P = 4*5*R2; This precomputation allows simultaneous reduction and multiplication.
// This is not the standard 'multiply-upper-by-5', here is why the factor is 4*5.
// For example, partial product (a2×r2):
//    (a2×2^88)×(r2×2^88) mod 2^130-5
//                                    = (a2×r2 × 2^176) mod 2^130-5
//                                    = (a2×r2 × 2^46×2^130) mod 2^130-5
//                                    = (a2×r2×2^46 × 2^130- 5×a2×r2×2^46 + 5×a2×r2×2^46) mod 2^130-5
//                                    = (a2×r2×2^46 × (2^130- 5) + 5×a2×r2×2^46) mod 2^130-5 // i.e. adding multiples of modulus is a noop
//                                    = (5×a2×r2×2^46) mod 2^130-5
//                                    = (a2×5×r2×2^2 × 2^44) mod 2^130-5 // Align to limb boudary
//                                    = (a2×[5×r2×4] × 2^44) mod 2^130-5
//                                    = (a2×R2P × 2^44) mod 2^130-5 // i.e. R2P = 4*5*R2
//
// See "Cheating at modular arithmetic" and "Poly1305's prime: 2^130 - 5"
//      paragraphs at https://loup-vaillant.fr/tutorials/poly1305-design for more details.
void MacroAssembler::poly1305_multiply8_avx512(
  const XMMRegister A0, const XMMRegister A1, const XMMRegister A2,   
  const XMMRegister R0, const XMMRegister R1, const XMMRegister R2, const XMMRegister R1P, const XMMRegister R2P, const Register polyCP) {
 
  const XMMRegister P0_L = xmm5;  // p[0] of the 8 blocks
  const XMMRegister P0_H = xmm6;  // p[0] of the 8 blocks
  const XMMRegister P1_L = xmm7;  // p[1] of the 8 blocks
  const XMMRegister P1_H = xmm8;  // p[1] of the 8 blocks 
  const XMMRegister P2_L = xmm9;  // p[2] of the 8 blocks
  const XMMRegister P2_H = xmm10; // p[2] of the 8 blocks
  const XMMRegister ZTMP1 = xmm11;

  // Reset partial sums
  evpxorq(P0_L, P0_L, P0_L, Assembler::AVX_512bit);
  evpxorq(P0_H, P0_H, P0_H, Assembler::AVX_512bit);
  evpxorq(P1_L, P1_L, P1_L, Assembler::AVX_512bit);
  evpxorq(P1_H, P1_H, P1_H, Assembler::AVX_512bit);
  evpxorq(P2_L, P2_L, P2_L, Assembler::AVX_512bit);
  evpxorq(P2_H, P2_H, P2_H, Assembler::AVX_512bit);

  // Calculate partial products
  evpmadd52luq(P0_L, A2, R1P, Assembler::AVX_512bit);
  evpmadd52huq(P0_H, A2, R1P, Assembler::AVX_512bit);
  evpmadd52luq(P1_L, A2, R2P, Assembler::AVX_512bit);
  evpmadd52huq(P1_H, A2, R2P, Assembler::AVX_512bit);
  evpmadd52luq(P2_L, A2, R0, Assembler::AVX_512bit);
  evpmadd52huq(P2_H, A2, R0, Assembler::AVX_512bit);

  evpmadd52luq(P1_L, A0, R1, Assembler::AVX_512bit);
  evpmadd52huq(P1_H, A0, R1, Assembler::AVX_512bit);
  evpmadd52luq(P2_L, A0, R2, Assembler::AVX_512bit);
  evpmadd52huq(P2_H, A0, R2, Assembler::AVX_512bit);
  evpmadd52luq(P0_L, A0, R0, Assembler::AVX_512bit);
  evpmadd52huq(P0_H, A0, R0, Assembler::AVX_512bit);

  evpmadd52luq(P0_L, A1, R2P, Assembler::AVX_512bit);
  evpmadd52huq(P0_H, A1, R2P, Assembler::AVX_512bit);
  evpmadd52luq(P1_L, A1, R0, Assembler::AVX_512bit);
  evpmadd52huq(P1_H, A1, R0, Assembler::AVX_512bit);
  evpmadd52luq(P2_L, A1, R1, Assembler::AVX_512bit);
  evpmadd52huq(P2_H, A1, R1, Assembler::AVX_512bit);

  // Carry propagation (first pass)
  vpsrlq(ZTMP1, P0_L, 44, Assembler::AVX_512bit);
  vpandq(A0, P0_L, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
  vpsllq(P0_H, P0_H, 8, Assembler::AVX_512bit);
  vpaddq(P0_H, P0_H, ZTMP1, Assembler::AVX_512bit);
  vpaddq(P1_L, P1_L, P0_H, Assembler::AVX_512bit);
  vpandq(A1, P1_L, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
  vpsrlq(ZTMP1, P1_L, 44, Assembler::AVX_512bit);
  vpsllq(P1_H, P1_H, 8, Assembler::AVX_512bit);
  vpaddq(P1_H, P1_H, ZTMP1, Assembler::AVX_512bit);
  vpaddq(P2_L, P2_L, P1_H, Assembler::AVX_512bit);
  vpandq(A2, P2_L, Address(polyCP, mask_42), Assembler::AVX_512bit); // Clear top 22 bits
  vpsrlq(ZTMP1, P2_L, 42, Assembler::AVX_512bit);
  vpsllq(P2_H, P2_H, 10, Assembler::AVX_512bit);
  vpaddq(P2_H, P2_H, ZTMP1, Assembler::AVX_512bit);

  // Carry propagation (second pass)
  // Multiply by 5 the highest bits (above 130 bits)
  vpaddq(A0, A0, P2_H, Assembler::AVX_512bit);
  vpsllq(P2_H, P2_H, 2, Assembler::AVX_512bit);
  vpaddq(A0, A0, P2_H, Assembler::AVX_512bit);
  vpsrlq(ZTMP1, A0, 44, Assembler::AVX_512bit);
  vpandq(A0, A0, Address(polyCP, mask_44), Assembler::AVX_512bit);
  vpaddq(A1, A1, ZTMP1, Assembler::AVX_512bit);
}

// Compute product for 16 16-byte message blocks
// This is poly1305_multiply8_avx512 twice
void MacroAssembler::poly1305_multiply16_avx512(
  const XMMRegister A0, const XMMRegister A1, const XMMRegister A2,
  const XMMRegister B0, const XMMRegister B1, const XMMRegister B2,
  const XMMRegister R0, const XMMRegister R1, const XMMRegister R2, const XMMRegister R1P, const XMMRegister R2P,
  const XMMRegister S0, const XMMRegister S1, const XMMRegister S2, const XMMRegister S1P, const XMMRegister S2P, const Register polyCP) {

  const XMMRegister P0_L = xmm0;  // p[0] of the 8 blocks
  const XMMRegister P0_H = xmm1;  // p[0] of the 8 blocks
  const XMMRegister P1_L = xmm2;  // p[1] of the 8 blocks
  const XMMRegister P1_H = xmm3;  // p[1] of the 8 blocks
  const XMMRegister P2_L = xmm4;  // p[2] of the 8 blocks
  const XMMRegister P2_H = xmm5;  // p[2] of the 8 blocks
  const XMMRegister Q0_L = xmm6;  // p[0] of the 8 blocks
  const XMMRegister Q0_H = xmm7;  // p[0] of the 8 blocks
  const XMMRegister Q1_L = xmm8;  // p[1] of the 8 blocks
  const XMMRegister Q1_H = xmm9;  // p[1] of the 8 blocks
  const XMMRegister Q2_L = xmm10; // p[2] of the 8 blocks
  const XMMRegister Q2_H = xmm11; // p[2] of the 8 blocks
  const XMMRegister ZTMP1 = xmm12;
  const XMMRegister ZTMP2 = xmm29;

  // Reset accumulator
  evpxorq(P0_L, P0_L, P0_L, Assembler::AVX_512bit);
  evpxorq(P0_H, P0_H, P0_H, Assembler::AVX_512bit);
  evpxorq(P1_L, P1_L, P1_L, Assembler::AVX_512bit);
  evpxorq(P1_H, P1_H, P1_H, Assembler::AVX_512bit);
  evpxorq(P2_L, P2_L, P2_L, Assembler::AVX_512bit);
  evpxorq(P2_H, P2_H, P2_H, Assembler::AVX_512bit);
  evpxorq(Q0_L, Q0_L, Q0_L, Assembler::AVX_512bit);
  evpxorq(Q0_H, Q0_H, Q0_H, Assembler::AVX_512bit);
  evpxorq(Q1_L, Q1_L, Q1_L, Assembler::AVX_512bit);
  evpxorq(Q1_H, Q1_H, Q1_H, Assembler::AVX_512bit);
  evpxorq(Q2_L, Q2_L, Q2_L, Assembler::AVX_512bit);
  evpxorq(Q2_H, Q2_H, Q2_H, Assembler::AVX_512bit);

  // Calculate products
  evpmadd52luq(P0_L, A2, R1P, Assembler::AVX_512bit);
  evpmadd52huq(P0_H, A2, R1P, Assembler::AVX_512bit);

  evpmadd52luq(Q0_L, B2, S1P, Assembler::AVX_512bit);
  evpmadd52huq(Q0_H, B2, S1P, Assembler::AVX_512bit);

  evpmadd52luq(P1_L, A2, R2P, Assembler::AVX_512bit);
  evpmadd52huq(P1_H, A2, R2P, Assembler::AVX_512bit);

  evpmadd52luq(Q1_L, B2, S2P, Assembler::AVX_512bit);
  evpmadd52huq(Q1_H, B2, S2P, Assembler::AVX_512bit);

  evpmadd52luq(P0_L, A0, R0, Assembler::AVX_512bit);
  evpmadd52huq(P0_H, A0, R0, Assembler::AVX_512bit);

  evpmadd52luq(Q0_L, B0, S0, Assembler::AVX_512bit);
  evpmadd52huq(Q0_H, B0, S0, Assembler::AVX_512bit);

  evpmadd52luq(P2_L, A2, R0, Assembler::AVX_512bit);
  evpmadd52huq(P2_H, A2, R0, Assembler::AVX_512bit);
  evpmadd52luq(Q2_L, B2, S0, Assembler::AVX_512bit);
  evpmadd52huq(Q2_H, B2, S0, Assembler::AVX_512bit);

  evpmadd52luq(P1_L, A0, R1, Assembler::AVX_512bit);
  evpmadd52huq(P1_H, A0, R1, Assembler::AVX_512bit);
  evpmadd52luq(Q1_L, B0, S1, Assembler::AVX_512bit);
  evpmadd52huq(Q1_H, B0, S1, Assembler::AVX_512bit);

  evpmadd52luq(P0_L, A1, R2P, Assembler::AVX_512bit);
  evpmadd52huq(P0_H, A1, R2P, Assembler::AVX_512bit);

  evpmadd52luq(Q0_L, B1, S2P, Assembler::AVX_512bit);
  evpmadd52huq(Q0_H, B1, S2P, Assembler::AVX_512bit);

  evpmadd52luq(P2_L, A0, R2, Assembler::AVX_512bit);
  evpmadd52huq(P2_H, A0, R2, Assembler::AVX_512bit);

  evpmadd52luq(Q2_L, B0, S2, Assembler::AVX_512bit);
  evpmadd52huq(Q2_H, B0, S2, Assembler::AVX_512bit);

  // Carry propagation (first pass)
  vpsrlq(ZTMP1, P0_L, 44, Assembler::AVX_512bit);
  vpsllq(P0_H, P0_H, 8, Assembler::AVX_512bit);
  vpsrlq(ZTMP2, Q0_L, 44, Assembler::AVX_512bit);
  vpsllq(Q0_H, Q0_H, 8, Assembler::AVX_512bit);

  evpmadd52luq(P1_L, A1, R0, Assembler::AVX_512bit);
  evpmadd52huq(P1_H, A1, R0, Assembler::AVX_512bit);
  evpmadd52luq(Q1_L, B1, S0, Assembler::AVX_512bit);
  evpmadd52huq(Q1_H, B1, S0, Assembler::AVX_512bit);

  // Carry propagation (first pass) - continue
  vpandq(A0, P0_L, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
  vpaddq(P0_H, P0_H, ZTMP1, Assembler::AVX_512bit);
  vpandq(B0, Q0_L, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
  vpaddq(Q0_H, Q0_H, ZTMP2, Assembler::AVX_512bit);

  evpmadd52luq(P2_L, A1, R1, Assembler::AVX_512bit);
  evpmadd52huq(P2_H, A1, R1, Assembler::AVX_512bit);
  evpmadd52luq(Q2_L, B1, S1, Assembler::AVX_512bit);
  evpmadd52huq(Q2_H, B1, S1, Assembler::AVX_512bit);

  // Carry propagation (first pass) - continue
  vpaddq(P1_L, P1_L, P0_H, Assembler::AVX_512bit);
  vpsllq(P1_H, P1_H, 8, Assembler::AVX_512bit);
  vpsrlq(ZTMP1, P1_L, 44, Assembler::AVX_512bit);
  vpandq(A1, P1_L, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
  vpaddq(Q1_L, Q1_L, Q0_H, Assembler::AVX_512bit);
  vpsllq(Q1_H, Q1_H, 8, Assembler::AVX_512bit);
  vpsrlq(ZTMP2, Q1_L, 44, Assembler::AVX_512bit);
  vpandq(B1, Q1_L, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits

  vpaddq(P2_L, P2_L, P1_H, Assembler::AVX_512bit);            // P2_L += P1_H + P1_L[63:44]
  vpaddq(P2_L, P2_L, ZTMP1, Assembler::AVX_512bit);
  vpandq(A2, P2_L, Address(polyCP, mask_42), Assembler::AVX_512bit); // Clear top 22 bits
  vpsrlq(ZTMP1, P2_L, 42, Assembler::AVX_512bit);
  vpsllq(P2_H, P2_H, 10, Assembler::AVX_512bit);
  vpaddq(P2_H, P2_H, ZTMP1, Assembler::AVX_512bit);

  vpaddq(Q2_L, Q2_L, Q1_H, Assembler::AVX_512bit);              // Q2_L += P1_H + P1_L[63:44]
  vpaddq(Q2_L, Q2_L, ZTMP2, Assembler::AVX_512bit);
  vpandq(B2, Q2_L, Address(polyCP, mask_42), Assembler::AVX_512bit); // Clear top 22 bits
  vpsrlq(ZTMP2, Q2_L, 42, Assembler::AVX_512bit);
  vpsllq(Q2_H, Q2_H, 10, Assembler::AVX_512bit);
  vpaddq(Q2_H, Q2_H, ZTMP2, Assembler::AVX_512bit);

  // ; Carry propagation (second pass)
  // ; Multiply by 5 the highest bits (above 130 bits)
  vpaddq(A0, A0, P2_H, Assembler::AVX_512bit);
  vpsllq(P2_H, P2_H, 2, Assembler::AVX_512bit);
  vpaddq(A0, A0, P2_H, Assembler::AVX_512bit);
  vpaddq(B0, B0, Q2_H, Assembler::AVX_512bit);
  vpsllq(Q2_H, Q2_H, 2, Assembler::AVX_512bit);
  vpaddq(B0, B0, Q2_H, Assembler::AVX_512bit);

  vpsrlq(ZTMP1, A0, 44, Assembler::AVX_512bit);
  vpandq(A0, A0, Address(polyCP, mask_44), Assembler::AVX_512bit);
  vpaddq(A1, A1, ZTMP1, Assembler::AVX_512bit);
  vpsrlq(ZTMP2, B0, 44, Assembler::AVX_512bit);
  vpandq(B0, B0, Address(polyCP, mask_44), Assembler::AVX_512bit);
  vpaddq(B1, B1, ZTMP2, Assembler::AVX_512bit);
}

// Compute product for a single 16-byte message blocks
//
// Math Note 1: Reduction by 2^130-5 can be expressed as follows:
//    ( a×2^130 + b ) mod 2^130-5 
//                                 = ( a×2^130 - 5×a + 5×a + b ) mod 2^130-5 
//                                 = ( a×(2^130 - 5) + 5×a + b ) mod 2^130-5 // i.e. adding multiples of modulus is a noop
//                                 = ( 5×a + b ) mod 2^130-5
// proves the well known algorithm of 'split the number down the middle, multiply upper and add'
// See "Cheating at modular arithmetic" and "Poly1305's prime: 2^130 - 5"
//      paragraphs at https://loup-vaillant.fr/tutorials/poly1305-design for more details.
//
// Note 2: A2 here is only two bits so anything above is subject of reduction.
//         Constant C1 = 5*R1 = R1 + (R1 << 2) simplifies multiply with less operations
//
// Flow of the code below is as follows:
//
//          A2        A1        A0
//        x           R1        R0
//   -----------------------------
//       A2×R0     A1×R0     A0×R0
//   +             A0×R1
//   +           5xA2xR1   5xA1xR1
//   -----------------------------
//     [0|L2L] [L1H|L1L] [L0H|L0L]
//
//   Registers:  T3:T2     T1:A0
//
// Completing the multiply and adding (with carry) 3x128-bit limbs into
// 192-bits again (3x64-bits):
// A0 = L0L
// A1 = L0H + L1L
// T3 = L1H + L2L
void MacroAssembler::poly1305_multiply_scalar(
    const Register A0, const Register A1, const Register A2,
    const Register R0, const Register R1, const Register C1, bool only128)
{
    const Register T1 = r13;    
    const Register T2 = r14;    
    const Register T3 = r15;
    // Note mulq instruction requires/clobers rax, rdx

    // T3:T2 = (A0 * R1)
    movq(rax, R1);
    mulq(A0);
    movq(T2, rax);
    movq(T3, rdx);

    // T1:A0 = (A0 * R0)
    movq(rax, R0);
    mulq(A0);
    movq(A0, rax); // A0 not used in other operations
    movq(T1, rdx);

    // T3:T2 += (A1 * R0)
    movq(rax, R0);
    mulq(A1);
    addq(T2, rax);
    adcq(T3, rdx);

    // T1:A0 += (A1 * R1x5)
    movq(rax, C1);
    mulq(A1);
    addq(A0, rax);
    adcq(T1, rdx);

    // Note: A2 is clamped to 2-bits,
    //       R1/R0 is clamped to 60-bits,
    //       their product is less than 2^64.

    if (only128) { // Accumulator only 128 bits, i.e. A2 == 0
      // just move and add T1-T2 to A1
      movq(A1, T1);
      addq(A1, T2);
      adcq(T3, 0);
    } else {
      // T3:T2 += (A2 * R1x5)
      movq(A1, A2); // use A1 for A2
      imulq(A1, C1);
      addq(T2, A1);
      adcq(T3, 0);

      movq(A1, T1); // T1:A0 => A1:A0

      // T3:A1 += (A2 * R0):T2
      imulq(A2, R0);
      addq(A1, T2);
      adcq(T3, A2);
    }

    // At this point, 3 64-bit limbs are in T3:A1:A0
    // T3 can span over more than 2 bits so final partial reduction step is needed.
    //
    // Partial reduction (just to fit into 130 bits)
    //    A2 = T3 & 3
    //    k = (T3 & ~3) + (T3 >> 2)
    //         Y    x4  +  Y    x1
    //    A2:A1:A0 += k
    //
    // Result will be in A2:A1:A0
    movq(T1, T3);
    movl(A2, T3); // DWORD
    andq (T1, ~3);
    shrq(T3, 2);
    addq(T1, T3);
    andl(A2, 3); // DWORD

    // A2:A1:A0 += k (kept in T1)
    addq(A0, T1);
    adcq(A1, 0);
    adcl(A2, 0); // DWORD
  }

  // Convert stream of quadwords in D0:D1 into 128-bit numbers across 44-bit limbs L0:L1:L2
  // Optionally pad all the numbers (i.e. add 2^128)
  void MacroAssembler::poly1305_limbs_avx512(
      const XMMRegister D0, const XMMRegister D1,
      const XMMRegister L0, const XMMRegister L1, const XMMRegister L2, bool padMSG, const Register polyCP)
  {
    const XMMRegister tmp1 = xmm0;
    const XMMRegister tmp2 = xmm1;
    // Interleave blocks of data
    evpunpckhqdq(tmp1, D0, D1, Assembler::AVX_512bit);
    evpunpcklqdq(L0, D0, D1, Assembler::AVX_512bit);

    // Highest 42-bit limbs of new blocks
    vpsrlq(L2, tmp1, 24, Assembler::AVX_512bit);
    if (padMSG) {
      vporq(L2, L2, Address(polyCP, high_bit), Assembler::AVX_512bit); // Add 2^128 to all 8 final qwords of the message
    }

    // Middle 44-bit limbs of new blocks
    vpsrlq(L1, L0, 44, Assembler::AVX_512bit);
    vpsllq(tmp2, tmp1, 20, Assembler::AVX_512bit);
    vpternlogq(L1, 0xA8, tmp2, Address(polyCP, mask_44), Assembler::AVX_512bit); // (A OR B AND C)

    // Lowest 44-bit limbs of new blocks 
    vpandq(L0, L0, Address(polyCP, mask_44), Assembler::AVX_512bit);
  }

  void MacroAssembler::poly1305_process_blocks_avx512(const Register input, const Register length, 
    const Register A0, const Register A1, const Register A2,
    const Register R0, const Register R1, const Register C1)
  {
    Label L_process256Loop, L_process256LoopDone;
    const Register T0 = r14;
    const Register T1 = r13;
    const Register polyCP = r13; //fixme: better register alloc?

    subq(rsp, 512/8*6); // Make room to store 6 zmm registers (powers of R)

    lea(polyCP, ExternalAddress(StubRoutines::x86::poly1305_mask_addr()));

    // Spread accumulator into 44-bit limbs in quadwords {xmm5,xmm6,xmm7}
    movq(T0, A0);
    andq(T0, Address(polyCP, mask_44)); // First limb (A[43:0])
    movq(xmm5, T0);

    movq(T0, A1);
    shrdq(A0, T0, 44);
    andq(A0, Address(polyCP, mask_44)); // Second limb (A[77:52])
    movq(xmm6, A0);

    shrdq(A1, A2, 24);
    andq(A1, Address(polyCP, mask_42)); // Third limb (A[129:88])
    movq(xmm7, A1);

    // To add accumulator, we must unroll first loop iteration 

    // Load first block of data (128 bytes) and pad
    // zmm13 to have bits 0-43 of all 8 blocks in 8 qwords
    // zmm14 to have bits 87-44 of all 8 blocks in 8 qwords
    // zmm15 to have bits 127-88 of all 8 blocks in 8 qwords
    evmovdquq(xmm13, Address(input, 0), Assembler::AVX_512bit);
    evmovdquq(xmm14, Address(input, 64), Assembler::AVX_512bit);
    poly1305_limbs_avx512(xmm13, xmm14, xmm13, xmm14, xmm15, true, polyCP);

    // Add accumulator to the fist message block
    vpaddq(xmm13, xmm13, xmm5, Assembler::AVX_512bit);
    vpaddq(xmm14, xmm14, xmm6, Assembler::AVX_512bit);
    vpaddq(xmm15, xmm15, xmm7, Assembler::AVX_512bit);

    // Load next blocks of data (128 bytes)  and pad
    // zmm16 to have bits 0-43 of all 8 blocks in 8 qwords
    // zmm17 to have bits 87-44 of all 8 blocks in 8 qwords
    // zmm18 to have bits 127-88 of all 8 blocks in 8 qwords
    evmovdquq(xmm16, Address(input, 64*2), Assembler::AVX_512bit);
    evmovdquq(xmm17, Address(input, 64*3), Assembler::AVX_512bit);
    poly1305_limbs_avx512(xmm16, xmm17, xmm16, xmm17, xmm18, true, polyCP);

    subl(length, 16*16);
    lea(input, Address(input,16*16)); 

    // Compute the powers of R^1..R^4 to form 44-bit limbs (half-empty)
    // zmm1 to have bits 0-127 in 4 quadword pairs
    // zmm2 to have bits 128-129 in alternating 8 qwords
    movq(xmm3, R0);
    vpinsrq(xmm3, xmm3, R1, 1);
    vinserti32x4(xmm1, xmm1, xmm3, 3);

    vpxorq(xmm2, xmm2, xmm2, Assembler::AVX_512bit);

    // Calculate R^2 
    movq(A0, R0);
    movq(A1, R1);
    // "Clever": A2 not set because poly1305_multiply_scalar has a flag to indicate 128-bit accumulator
    poly1305_multiply_scalar(A0, A1, A2, R0, R1, C1, true);

    movq(xmm3, A0);
    vpinsrq(xmm3, xmm3, A1, 1);
    vinserti32x4(xmm1, xmm1, xmm3, 2);

    movq(xmm4, A2);
    vinserti32x4(xmm2, xmm2, xmm4, 2);

    // Calculate R^3
    poly1305_multiply_scalar(A0, A1, A2, R0, R1, C1, false);

    movq(xmm3, A0);
    vpinsrq(xmm3, xmm3, A1, 1);
    vinserti32x4(xmm1, xmm1, xmm3, 1);
    movq(xmm4, A2);
    vinserti32x4(xmm2, xmm2, xmm4, 1);

    // Calculate R^4
    poly1305_multiply_scalar(A0, A1, A2, R0, R1, C1, false);

    movq(xmm3, A0);
    vpinsrq(xmm3, xmm3, A1, 1);
    vinserti32x4(xmm1, xmm1, xmm3, 0);
    movq(xmm4, A2);
    vinserti32x4(xmm2, xmm2, xmm4, 0);

    // Interleave the powers of R^1..R^4 to form 44-bit limbs (half-empty)
    // zmm19 to have bits 0-43 of all 4 blocks in alternating 8 qwords
    // zmm20 to have bits 87-44 of all 4 blocks in alternating 8 qwords
    // zmm21 to have bits 127-88 of all 4 blocks in alternating 8 qwords
    lea(polyCP, ExternalAddress(StubRoutines::x86::poly1305_mask_addr()));
    vpxorq(xmm20, xmm20, xmm20, Assembler::AVX_512bit);
    poly1305_limbs_avx512(xmm1, xmm20, xmm19, xmm20, xmm21, false, polyCP);

    // zmm2 contains the 2 highest bits of the powers of R
    vpsllq(xmm2, xmm2, 40, Assembler::AVX_512bit);
    vporq(xmm21, xmm21, xmm2, Assembler::AVX_512bit);


    // Broadcast 44-bit limbs of R^4 into {zmm24,zmm23,zmm22}
    mov(T0, A0);
    andq(T0, Address(polyCP, mask_44)); // First limb (R^4[43:0])
    evpbroadcastq(xmm22, T0, Assembler::AVX_512bit);

    movq(T0, A1);
    shrdq(A0, T0, 44);
    andq(A0, Address(polyCP, mask_44)); // Second limb (R^4[87:44])
    evpbroadcastq(xmm23, A0, Assembler::AVX_512bit);

    shrdq(A1, A2, 24);
    andq(A1, Address(polyCP, mask_42)); // Third limb (R^4[129:88])
    evpbroadcastq(xmm24, A1, Assembler::AVX_512bit);

    // Generate 4*5*R^4 into {xmm26,xmm25}
    // Used as multiplier in poly1305_multiply8_avx512 so can
    // ignore bottom limb and carry propagation
    vpsllq(xmm25, xmm23, 2, Assembler::AVX_512bit);     // 4*R^4
    vpsllq(xmm26, xmm24, 2, Assembler::AVX_512bit);
    vpaddq(xmm25, xmm25, xmm23, Assembler::AVX_512bit); // 5*R^4
    vpaddq(xmm26, xmm26, xmm24, Assembler::AVX_512bit);
    vpsllq(xmm25, xmm25, 2, Assembler::AVX_512bit);     // 4*5*R^4
    vpsllq(xmm26, xmm26, 2, Assembler::AVX_512bit);

    // Move R^4..R^1 one element over
    vpslldq(xmm29, xmm19, 8, Assembler::AVX_512bit);
    vpslldq(xmm30, xmm20, 8, Assembler::AVX_512bit);
    vpslldq(xmm31, xmm21, 8, Assembler::AVX_512bit);

    // Calculate R^8-R^5
    poly1305_multiply8_avx512(xmm19, xmm20, xmm21,               // ACC=R^4..R^1
                              xmm22, xmm23, xmm24, xmm25, xmm26, // R^4..R^4, 4*5*R^4
                              polyCP);

    // Interleave powers of R: R^8 R^4 R^7 R^3 R^6 R^2 R^5 R
    vporq(xmm19, xmm19, xmm29, Assembler::AVX_512bit);
    vporq(xmm20, xmm20, xmm30, Assembler::AVX_512bit);
    vporq(xmm21, xmm21, xmm31, Assembler::AVX_512bit);

    // Broadcast R^8
    vpbroadcastq(xmm22, xmm19, Assembler::AVX_512bit);
    vpbroadcastq(xmm23, xmm20, Assembler::AVX_512bit);
    vpbroadcastq(xmm24, xmm21, Assembler::AVX_512bit);

    // Generate 4*5*R^8
    vpsllq(xmm25, xmm23, 2, Assembler::AVX_512bit);
    vpsllq(xmm26, xmm24, 2, Assembler::AVX_512bit);
    vpaddq(xmm25, xmm25, xmm23, Assembler::AVX_512bit); // 5*R^8
    vpaddq(xmm26, xmm26, xmm24, Assembler::AVX_512bit);
    vpsllq(xmm25, xmm25, 2, Assembler::AVX_512bit);     // 4*5*R^8
    vpsllq(xmm26, xmm26, 2, Assembler::AVX_512bit);

    // Store R^8-R for later use
    evmovdquq(Address(rsp, 64*0), xmm19, Assembler::AVX_512bit);
    evmovdquq(Address(rsp, 64*1), xmm20, Assembler::AVX_512bit);
    evmovdquq(Address(rsp, 64*2), xmm21, Assembler::AVX_512bit);

    // Calculate R^16-R^9
    poly1305_multiply8_avx512(xmm19, xmm20, xmm21,               // ACC=R^8..R^1
                              xmm22, xmm23, xmm24, xmm25, xmm26, // R^8..R^8, 4*5*R^8
                              polyCP);

    // Store R^16-R^9 for later use
    evmovdquq(Address(rsp, 64*3), xmm19, Assembler::AVX_512bit);
    evmovdquq(Address(rsp, 64*4), xmm20, Assembler::AVX_512bit);
    evmovdquq(Address(rsp, 64*5), xmm21, Assembler::AVX_512bit);

    // Broadcast R^16
    vpbroadcastq(xmm22, xmm19, Assembler::AVX_512bit);
    vpbroadcastq(xmm23, xmm20, Assembler::AVX_512bit);
    vpbroadcastq(xmm24, xmm21, Assembler::AVX_512bit);

    // Generate 4*5*R^16
    vpsllq(xmm25, xmm23, 2, Assembler::AVX_512bit);
    vpsllq(xmm26, xmm24, 2, Assembler::AVX_512bit);
    vpaddq(xmm25, xmm25, xmm23, Assembler::AVX_512bit); // 5*R^16
    vpaddq(xmm26, xmm26, xmm24, Assembler::AVX_512bit);
    vpsllq(xmm25, xmm25, 2, Assembler::AVX_512bit);     // 4*5*R^16
    vpsllq(xmm26, xmm26, 2, Assembler::AVX_512bit);

    // VECTOR LOOP: process 16 * 16-byte message block at a time 
    bind(L_process256Loop);
    cmpl(length, 16*16);
    jcc(Assembler::less, L_process256LoopDone);

    poly1305_multiply16_avx512(xmm13, xmm14, xmm15, xmm16, xmm17, xmm18, // MSG/ACC 16 blocks
                               xmm22, xmm23, xmm24, xmm25, xmm26,  //R^16..R^16, 4*5*R^16
                               xmm22, xmm23, xmm24, xmm25, xmm26,  //R^16..R^16, 4*5*R^16
                               polyCP);
    {  //FIXME: remove code-block; measure perf if placed before poly1305_multiply16_avx512
        const XMMRegister ZTMP2 = xmm2; //xmm31;
        const XMMRegister ZTMP5 = xmm3;
        const XMMRegister ZTMP6 = xmm4;
        const XMMRegister ZTMP7 = xmm5;
        const XMMRegister ZTMP8 = xmm6;
        const XMMRegister ZTMP9 = xmm7; //xmm12;    

        // Load and interleave next block of data (128 bytes)
        evmovdquq(ZTMP5, Address(input, 0), Assembler::AVX_512bit);
        evmovdquq(ZTMP2, Address(input, 64), Assembler::AVX_512bit);
        poly1305_limbs_avx512(ZTMP5, ZTMP2, ZTMP5, ZTMP2, ZTMP6, true, polyCP); //{ZTMP6,ZTMP2,ZTMP5}

        // Load and interleave next block of data (128 bytes)
        evmovdquq(ZTMP8, Address(input, 64*2), Assembler::AVX_512bit);
        evmovdquq(ZTMP9, Address(input, 64*3), Assembler::AVX_512bit);
        poly1305_limbs_avx512(ZTMP8, ZTMP9, ZTMP8, ZTMP9, ZTMP7, true, polyCP); //{ZTMP7,ZTMP9,ZTMP8}

        vpaddq(xmm15, xmm15, ZTMP6, Assembler::AVX_512bit); //Add highest bits from new blocks to accumulator
        vpaddq(xmm18, xmm18, ZTMP7, Assembler::AVX_512bit); // Add highest bits from new blocks to accumulator
        vpaddq(xmm13, xmm13, ZTMP5, Assembler::AVX_512bit); // Add low 42-bit bits from new blocks to accumulator
        vpaddq(xmm14, xmm14, ZTMP2, Assembler::AVX_512bit); // Add medium 42-bit bits from new blocks to accumulator
        vpaddq(xmm16, xmm16, ZTMP8, Assembler::AVX_512bit); // Add low 42-bit bits from new blocks to accumulator
        vpaddq(xmm17, xmm17, ZTMP9, Assembler::AVX_512bit); // Add medium 42-bit bits from new blocks to accumulator
    }

    subl(length, 16*16);
    lea(input, Address(input,16*16)); 
    jmp(L_process256Loop);

    bind(L_process256LoopDone);

    // Tail processing: Need to multiply ACC by R^16..R^1 and add it all up into a single scalar value
    // Read R^16-R^9
    evmovdquq(xmm19, Address(rsp, 64*3), Assembler::AVX_512bit);
    evmovdquq(xmm20, Address(rsp, 64*4), Assembler::AVX_512bit);
    evmovdquq(xmm21, Address(rsp, 64*5), Assembler::AVX_512bit);
    // Read R^8-R
    evmovdquq(xmm22, Address(rsp, 64*0), Assembler::AVX_512bit);
    evmovdquq(xmm23, Address(rsp, 64*1), Assembler::AVX_512bit);
    evmovdquq(xmm24, Address(rsp, 64*2), Assembler::AVX_512bit);
    
    // Generate 4*5*[R^16..R^9] (ignore lowest limb)
    vpsllq(xmm0, xmm20, 2, Assembler::AVX_512bit);
    vpaddq(xmm27, xmm20, xmm0, Assembler::AVX_512bit); // R1' (R1*5)
    vpsllq(xmm1, xmm21, 2, Assembler::AVX_512bit);
    vpaddq(xmm28, xmm21, xmm1, Assembler::AVX_512bit); // R2' (R2*5)
    vpsllq(xmm27, xmm27, 2, Assembler::AVX_512bit);    // 4*5*R
    vpsllq(xmm28, xmm28, 2, Assembler::AVX_512bit);

    // Generate 4*5*[R^8..R^1] (ignore lowest limb)
    vpsllq(xmm2, xmm23, 2, Assembler::AVX_512bit);
    vpaddq(xmm25, xmm23, xmm2, Assembler::AVX_512bit); // R1' (R1*5)
    vpsllq(xmm3, xmm24, 2, Assembler::AVX_512bit);
    vpaddq(xmm26, xmm24, xmm3, Assembler::AVX_512bit); // R2' (R2*5)
    vpsllq(xmm25, xmm25, 2, Assembler::AVX_512bit); // 4*5*R
    vpsllq(xmm26, xmm26, 2, Assembler::AVX_512bit);

    poly1305_multiply16_avx512(xmm13, xmm14, xmm15, xmm16, xmm17, xmm18, // MSG/ACC 16 blocks
                               xmm19, xmm20, xmm21, xmm27, xmm28,  // R^16-R^9, R1P, R2P
                               xmm22, xmm23, xmm24, xmm25, xmm26,  // R^8-R, R1P, R2P
                               polyCP);

    // Add all blocks (horizontally) 
    // 16->8 blocks
    vpaddq(xmm13, xmm13, xmm16, Assembler::AVX_512bit); 
    vpaddq(xmm14, xmm14, xmm17, Assembler::AVX_512bit);
    vpaddq(xmm15, xmm15, xmm18, Assembler::AVX_512bit);

    // 8 -> 4 blocks
    vextracti64x4(xmm0, xmm13, 1); //fix vector_len from here
    vextracti64x4(xmm1, xmm14, 1);
    vextracti64x4(xmm2, xmm15, 1);
    vpaddq(xmm13, xmm13, xmm0, Assembler::AVX_256bit);
    vpaddq(xmm14, xmm14, xmm1, Assembler::AVX_256bit);
    vpaddq(xmm15, xmm15, xmm2, Assembler::AVX_256bit);

    // 4 -> 2 blocks
    vextracti32x4(xmm10, xmm13, 1);
    vextracti32x4(xmm11, xmm14, 1);
    vextracti32x4(xmm12, xmm15, 1);
    vpaddq(xmm13, xmm13, xmm10, Assembler::AVX_128bit);
    vpaddq(xmm14, xmm14, xmm11, Assembler::AVX_128bit);
    vpaddq(xmm15, xmm15, xmm12, Assembler::AVX_128bit);

    // 2 -> 1 blocks
    vpsrldq(xmm10, xmm13, 8, Assembler::AVX_128bit);
    vpsrldq(xmm11, xmm14, 8, Assembler::AVX_128bit);
    vpsrldq(xmm12, xmm15, 8, Assembler::AVX_128bit);

    // Finish folding and clear second qword
    mov64(T0, 0xfd);
    kmovql(k1, T0);
    evpaddq(xmm13, k1, xmm13, xmm10, false, Assembler::AVX_512bit);
    evpaddq(xmm14, k1, xmm14, xmm11, false, Assembler::AVX_512bit);
    evpaddq(xmm15, k1, xmm15, xmm12, false, Assembler::AVX_512bit);

    // Carry propagation
    vpsrlq(xmm0, xmm13, 44, Assembler::AVX_512bit);
    vpandq(xmm13, xmm13, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
    vpaddq(xmm14, xmm14, xmm0, Assembler::AVX_512bit);
    vpsrlq(xmm0, xmm14, 44, Assembler::AVX_512bit);
    vpandq(xmm14, xmm14, Address(polyCP, mask_44), Assembler::AVX_512bit); // Clear top 20 bits
    vpaddq(xmm15, xmm15, xmm0, Assembler::AVX_512bit);
    vpsrlq(xmm0, xmm15, 42, Assembler::AVX_512bit);
    vpandq(xmm15, xmm15, Address(polyCP, mask_42), Assembler::AVX_512bit); // Clear top 22 bits
    vpsllq(xmm1, xmm0, 2, Assembler::AVX_512bit);
    vpaddq(xmm0, xmm0, xmm1, Assembler::AVX_512bit);
    vpaddq(xmm13, xmm13, xmm0, Assembler::AVX_512bit);

    // Put together A (accumulator)
    movq(A0, xmm13);

    movq(T0, xmm14);
    movq(T1, T0);
    shlq(T1, 44);
    orq(A0, T1);

    shrq(T0, 20);
    movq(A2, xmm15);
    movq(A1, A2);
    shlq(A1, 24);
    orq(A1, T0);
    shrq(A2, 40);

    // Return stack
    addq(rsp, 512/8*6); // (powers of R)
  }

  void MacroAssembler::poly1305_process_blocks(Register input, Register length, Register accumulator, Register R)
  {
    // const Register input        = rdi;
    // const Register length       = rbx;
    // const Register accumulator  = rcx;
    // const Register R            = r8;

    const Register A0 = rsi;  // [in/out] accumulator bits 63..0
    const Register A1 = r9;   // [in/out] accumulator bits 127..64
    const Register A2 = r10;  // [in/out] accumulator bits 195..128
    const Register R0 = r11;  // R constant bits 63..0
    const Register R1 = r12;  // R constant bits 127..64
    const Register C1 = r8;   // 5*R (upper limb only)

    Label L_process16Loop, L_process16LoopDone;

    // Load R
    movq(R0, Address(R, 0));
    movq(R1, Address(R, 8));

    // Compute 5*R (Upper limb only)
    movq(C1, R1);
    shrq(C1, 2);
    addq(C1, R1); // C1 = R1 + (R1 >> 2)

    // Load accumulator
    movq(A0, Address(accumulator, 0));
    movq(A1, Address(accumulator, 8));
    movzbq(A2, Address(accumulator, 16));
    
    // Minimum of 256 bytes to run vectorized code
    cmpl(length, 16*16);
    jcc(Assembler::less, L_process16Loop);

    poly1305_process_blocks_avx512(input, length, 
                                   A0, A1, A2,
                                   R0, R1, C1);

    // SCALAR LOOP: process one 16-byte message block at a time 
    bind(L_process16Loop);
    cmpl(length, 16);
    jcc(Assembler::less, L_process16LoopDone);

    addq(A0, Address(input,0));
    adcq(A1, Address(input,8));
    adcq(A2,1);
    poly1305_multiply_scalar(A0, A1, A2, R0, R1, C1, false);

    subl(length, 16);
    lea(input, Address(input,16));
    jmp(L_process16Loop);
    bind(L_process16LoopDone);

    // Write output
    movq(Address(accumulator, 0), A0);
    movq(Address(accumulator, 8), A1);
    movb(Address(accumulator, 16), A2);
  }

#endif // _LP64
