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
#include "macroAssembler_x86.hpp"
#include "stubGenerator_x86_64.hpp"

#define __ _masm->

ATTRIBUTE_ALIGNED(64) uint64_t MODULUS_P256[] = {
  0x000fffffffffffff, 0x00000fffffffffff, 
  0x0000000000000000, 0x0000001000000000, 
  0x0000ffffffff0000, 0x0000000000000000,
  0x0000000000000000, 0x0000000000000000
};
static address modulus_p256() {
  return (address)MODULUS_P256;
}

ATTRIBUTE_ALIGNED(64) uint64_t P256_MASK52[] = {
  0x001fffffffffffff, 0x001fffffffffffff,
  0x001fffffffffffff, 0x001fffffffffffff,
  0x001fffffffffffff, 0x001fffffffffffff,
  0x001fffffffffffff, 0x001fffffffffffff,
};
static address p256_mask52() {
  return (address)P256_MASK52;
}

void StubGenerator::montgomeryMultiply(const Register aLimbs, const Register bLimbs, const Register rLimbs) {
  /*
    M1 = load(*m)
    M2 = M1 shift one q element <<
    A1 = load(*a)
    A2 = A1 shift one q element <<
    Acc1 = 0
    ---- for i = 0 to 5
        Acc2 = 0
        B1[0] = load(b[i])
        B1 = B1[0] replicate q
        Acc1 += A1 *  B1
        Acc2 += A2 *h B1
        N = Acc1[0] replicate q
        Acc1 += M1  * N
        Acc2 += M1 *h N
        Acc1 = Acc1 shift one q element >>
        Acc1 = Acc1 + Acc2
    ---- done
  */    

  KRegister oneLimb = k1;
  KRegister allLimbs = k2;
  Register t0 = r13;
  Register rscratch = r13;
  XMMRegister modulus = xmm0;
  XMMRegister modulusH = xmm1;
  XMMRegister A = xmm2;
  XMMRegister AH = xmm3;
  XMMRegister Acc = xmm4;
  XMMRegister AccH = xmm5;
  XMMRegister B = xmm6;
  XMMRegister N = xmm7;
  XMMRegister mask = xmm8;
  XMMRegister carry = xmm8;
  
  __ mov64(t0, 0x1);
  __ kmovql(oneLimb, t0);
  __ mov64(t0, 0x1f);
  __ kmovql(allLimbs, t0);

  // M1 = load(*m)
  __ evmovdquq(modulus, allLimbs, ExternalAddress(modulus_p256()), false, Assembler::AVX_512bit, rscratch); //endiannes?

  // M2 = M1 shift one q element <<
  __ evpshrdq(modulusH, allLimbs, modulus, modulus, 1, false, Assembler::AVX_512bit);

  // A1 = load(*a)
  __ evmovdquq(A, allLimbs, Address(aLimbs, 0), false, Assembler::AVX_512bit);

  // A2 = A1 shift one q element <<
  __ evpshrdq(A, allLimbs, A, A, 1, false, Assembler::AVX_512bit);
  
  // Acc1 = 0
  __ vpxorq(Acc, Acc, Acc, Assembler::AVX_512bit);
  for (int i = 0; i< 5; i++) {
      // Acc2 = 0
      __ vpxorq(AccH, AccH, AccH, Assembler::AVX_512bit);

      // B1[0] = load(b[i])
      // B1 = B1[0] replicate q
      __ vpbroadcastq(B, Address(bLimbs, i*8), Assembler::AVX_512bit);

      // Acc1 += A1 *  B1
      __ evpmadd52luq(Acc, A, B, Assembler::AVX_512bit);
      
      // Acc2 += A2 *h B1
      __ evpmadd52huq(AccH, AH, B, Assembler::AVX_512bit);

      // N = Acc1[0] replicate q
      __ vpbroadcastq(N, A, Assembler::AVX_512bit);

      // N = N mask 52 // not needed, implied by IFMA
      // Acc1 += M1  * N
      __ evpmadd52luq(Acc, modulus, N, Assembler::AVX_512bit);

      // Acc2 += M2 *h N
      __ evpmadd52huq(AccH, modulusH, N, Assembler::AVX_512bit);

      // Acc1[0] =>> 52
      __ evpsrlq(Acc, oneLimb, Acc, 52, false, Assembler::AVX_512bit);

      // Acc2[0] += Acc1[0]
      __ evpaddq(AccH, oneLimb, Acc, AccH, false, Assembler::AVX_512bit);

      // Acc1 = Acc1 shift one q element >>
      __ evpshrdq(Acc, allLimbs, Acc, Acc, 1, false, Assembler::AVX_512bit);
      
      // Acc1 = Acc1 + Acc2
      __ vpaddq(Acc, Acc, AccH, Assembler::AVX_512bit);
  }
  // Acc2 = Acc1 - M1
  __ evpsubq(AccH, allLimbs, Acc, modulus, false, Assembler::AVX_512bit);
  
  for (int i = 0; i< 5; i++) {
    // Carry1 = Acc1>>52
    __ evpsrlq(carry, allLimbs, Acc, 52, false, Assembler::AVX_512bit);
    // Carry1 = shift one q element <<
    __ evpshldq(carry, allLimbs, carry, carry, 1, false, Assembler::AVX_512bit); // what about top limb?
    // Acc1 += Carry1
    __ evpandq(Acc, Acc, ExternalAddress(p256_mask52()), Assembler::AVX_512bit, rscratch); // Clear top 12 bits
    __ vpaddq(Acc, Acc, carry, Assembler::AVX_512bit);

    // Carry1 = Acc2>>52
    __ evpsrlq(carry, allLimbs, AccH, 52, false, Assembler::AVX_512bit);
    // Carry1 = shift one q element <<
    __ evpshldq(carry, allLimbs, carry, carry, 1, false, Assembler::AVX_512bit); // what about top limb?
    // Acc2 += Carry2
    __ evpandq(AccH, AccH, ExternalAddress(p256_mask52()), Assembler::AVX_512bit, rscratch); // Clear top 12 bits
    __ vpaddq(AccH, AccH, carry, Assembler::AVX_512bit);
  }
  // mask = broadcast Acc2[5]? use single permute instead with memory operand?
  __ evpshrdq(mask, allLimbs, AccH, AccH, 5, false, Assembler::AVX_512bit);
  __ vpbroadcastq(mask, mask, Assembler::AVX_512bit);

  // select on mask
  __ vpternlogq(Acc, 0xE4, AccH, mask, Assembler::AVX_512bit); // (C?A:B)
  // output to r
  // __ evpandq(Acc, Acc, ExternalAddress(p256_mask52()), Assembler::AVX_512bit, rscratch); // Clear top 12 bits, needed?
  __ evmovdquq(Address(rLimbs, 0), allLimbs, Acc, false, Assembler::AVX_512bit);
}

address StubGenerator::generate_intpoly_montgomeryMult_P256() {
  __ align(CodeEntryAlignment);
  StubCodeMark mark(this, "StubRoutines", "intpoly_montgomeryMult_P256");
  address start = __ pc();
  __ enter();

  // Save all 'SOE' registers
  __ push(rbx);
  #ifdef _WIN64
  __ push(rsi);
  __ push(rdi);
  #endif
  __ push(r12);
  __ push(r13);
  __ push(r14);
  __ push(r15);

  // Register Map
  const Register aLimbs  = rdi;
  const Register bLimbs  = rsi;
  const Register rLimbs  = rdx;

  // Normalize input
  // pseudo-signature: void poly1305_processBlocks(byte[] input, int length, int[5] accumulator, int[5] R)
  // a, b, r pointers point at first array element
  // java headers bypassed in LibraryCallKit::inline_poly1305_processBlocks
  #ifdef _WIN64
  // c_rarg0 - rcx
  // c_rarg1 - rdx
  // c_rarg2 - r8
  __ mov(aLimbs, c_rarg0);
  __ mov(bLimbs, c_rarg1);
  __ mov(rLimbs, c_rarg2);
  #else
  // Already in place
  // c_rarg0 - rdi
  // c_rarg1 - rsi
  // c_rarg2 - rdx
  #endif

  montgomeryMultiply(aLimbs, bLimbs, rLimbs);

  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  #ifdef _WIN64
  __ pop(rdi);
  __ pop(rsi);
  #endif
  __ pop(rbx);

  __ leave();
  __ ret(0);
  return start;
}
