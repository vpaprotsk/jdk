#ifdef VP_INCLUDE
//#if 1
/*
 * Copyright (c) 2023, Intel Corporation. All rights reserved.
 * Intel Math Library (LIBM) Source Code
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

/******************************************************************************/
//                     String handling intrinsics
//                     --------------------------
//
// Currently implementing strchr and strstr.  Used for IndexOf operations.
//
/******************************************************************************/

#define __ _masm->

//var_mov(needleChar, needleChar2, needle);
void var_mov(Register needleChar, Register needleChar2, Register needle, int size, MacroAssembler* _masm) {
    switch (size) {
        case 3: __ movzbl(needleChar, Address(needle, 1)); break;
        case 4: __ movzwl(needleChar, Address(needle, 1)); break;
        case 5:
        case 6: __ movl(needleChar, Address(needle, 1)); break;
        case 7: // FIXME! off-by-two?! 8 chacacter mov.. (read 2 extra) (overoptimized?)
        case 8: // FIXME! off-by-one?! 8 chacacter mov from inxex 1 (read 1 extra)
        case 9:
        case 10: __ movq(needleChar, Address(needle, 1)); break;
        
        case 11:
            __ movq(needleChar, Address(needle, 1));
            __ movzbl(needleChar2, Address(needle, 9));
            break;
        case 12:
            __ movq(needleChar, Address(needle, 1));
            __ movzwl(needleChar2, Address(needle, 9));
            break;
    }
}

//var_cmp_jmp(rdx, rdi, rsi, L_found, _masm); 
void var_cmp_je(Register strCharAddr, Register bitpos, Register needleChar, Register needleChar2, Label& L_found, int size, MacroAssembler* _masm) {
    Register tmp = needleChar2;
    switch (size) {
        case 3:  __ cmpb(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar); break;
        case 4:  __ cmpw(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar); break;
        case 5:
        case 6:  __ cmpl(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar); break;
        case 9:  __ cmpq(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar); break;
        case 10: __ cmpq(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar); break;
        
        case 7:
            __ movq(tmp, Address(strCharAddr, bitpos, Address::times_1, 0x1));
            __ xorq(tmp, needleChar);
            __ shlq(tmp, 0x18);  // ?? clearing extra? cmpq instead?
            break;
        case 8:
            __ movq(tmp, Address(strCharAddr, bitpos, Address::times_1, 0x1));
            __ xorq(tmp, needleChar);
            __ shlq(tmp, 0x10);   // ?? clearing extra? cmpq instead?
            break;
        case 11:
            __ cmpq(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar);
            __ je_b(L_found);
            __ cmpb(Address(strCharAddr, bitpos, Address::times_1, 0x9), needleChar2);
            break;
        case 12:
            __ cmpq(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar);
            __ je_b(L_found);
            __ cmpw(Address(strCharAddr, bitpos, Address::times_1, 0x9), needleChar2);
            break;
    }
    __ je(L_found);
}

/*
    const __m256i first = _mm256_set1_epi8(needle[0]);
    const __m256i last  = _mm256_set1_epi8(needle[k - 1]);

    for (size_t i = 0; i < n; i += 32) {

        const __m256i block_first = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i));
        const __m256i block_last  = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i + k - 1));

        const __m256i eq_first = _mm256_cmpeq_epi8(first, block_first);
        const __m256i eq_last  = _mm256_cmpeq_epi8(last, block_last);

        uint32_t mask = _mm256_movemask_epi8(_mm256_and_si256(eq_first, eq_last));

        while (mask != 0) {

            const auto bitpos = bits::get_first_bit_set(mask);

            if (memcmp_fun(s + i + bitpos + 1, needle + 1)) {
                return i + bitpos;
            }

            mask = bits::clear_leftmost_set(mask);
        }
    }

    return std::string::npos;
*/
    // __ movq(r12, rcx);  // rcx = len(needle)
    // __ movq(r11, rdx);  // rdx = needle
    // __ movq(r10, rsi);  // rsi = len(string)
    // __ movq(rbx, rdi);  // rdi = string
// case_avx2_strstr(rbx, r10, r11, needleLen, L_tail_3_9, rax, 
//                       rcx, rdx, rsi, rdi, r8, _masm)
address case_avx2_strstr(Register searchStr, Register strLen, Register needle, int needleLen, Label& breakSearch, 
        Register tmp1, Register tmp2, Register tmp3, Register tmp4, Register tmp5, Register tmp6, 
        MacroAssembler* _masm){
    Label L_entry, L_outer, L_mid, L_inner, L_found;
    Register loopIndex   = tmp1; //rax
    Register foundAt     = tmp2; //rcx
    Register strCharAddr = tmp3; //rdx 
    Register needleChar  = tmp4; //rsi
    Register bitpos      = tmp5; //rdi
    Register needleChar2 = tmp6; //r8
    
    XMMRegister first = xmm0;
    XMMRegister last  = xmm1;
    XMMRegister eq_first = xmm2;
    XMMRegister eq_last  = xmm3;

    __ bind(L_entry);
    address table_address = __ pc();

// jmp <_Z14avx2_strstr_v2PKcmS0_m+796> L_mid6
    __ vpbroadcastb(first, Address(needle, 0, Address::times_1), Assembler::AVX_256bit);
    __ vpbroadcastb(last,  Address(needle, needleLen-1, Address::times_1), Assembler::AVX_256bit);
    __ xorl(loopIndex, loopIndex); // i = 0
    __ jmpb(L_mid);

    __ align(16);
    __ bind(L_outer);
// jae <_Z14avx2_strstr_v2PKcmS0_m+1511> L_tail_3_9
    __ addq(loopIndex, 0x20);   // i += 32
    __ movq(foundAt, -1);
    __ cmpq(loopIndex, strLen);
    __ jae(breakSearch);  // if (i>=len(string)) break; //jccb(Assembler::aboveEqual, L) //jae vs jae_b?

    __ bind(L_mid);
// je <_Z14avx2_strstr_v2PKcmS0_m+776> L_outer6
    __ vpcmpeqb(eq_first, first, Address(searchStr, loopIndex, Address::times_1), Assembler::AVX_256bit);
    __ vpcmpeqb(eq_last, last, Address(searchStr, loopIndex, Address::times_1, needleLen-1), Assembler::AVX_256bit);
    __ vpand(eq_first, eq_last, eq_first, Assembler::AVX_256bit);
    __ vpmovmskb(foundAt, eq_first, Assembler::AVX_256bit);     // rcx = _mm256_movemask_epi8
    __ testl(foundAt, foundAt);
    __ je_b(L_outer);  // if (mask == 0) next; //jccb(Assembler::equal, L) // VP: really? rcx==rcx always true?

    __ leaq(strCharAddr, Address(searchStr, loopIndex, Address::times_1)); // string[i]
    var_mov(needleChar, needleChar2, needle, needleLen, _masm);
    // __ movl(needleChar, Address(needle, 1));                               // needle[1](char1-4) char0&char5 already equal

    __ align(16);
    __ bind(L_inner);
// je <_Z14avx2_strstr_v2PKcmS0_m+1270> L_0x404f26
// jne <_Z14avx2_strstr_v2PKcmS0_m+832> L_inner6
// jmp <_Z14avx2_strstr_v2PKcmS0_m+776> L_outer6
    //__ xorl(rdi, rdi);
    __ tzcntl(bitpos, foundAt);
    var_cmp_je(strCharAddr, bitpos, needleChar, needleChar2, L_found, needleLen, _masm);
    // __ cmpl(Address(strCharAddr, bitpos, Address::times_1, 0x1), needleChar);
    // __ je(L_found);

    __ blsrl(foundAt, foundAt);  // reset lowest bit set
    __ jne_b(L_inner); // next bit in mask
    __ jmpb(L_outer);  // next i

    __ bind(L_found);
    __ movl(foundAt, bitpos);
// jmp <_Z14avx2_strstr_v2PKcmS0_m+1505> L_tail_10_12
    
    // __ jmp(L_tail_8);
    // __ bind(L_tail_10_12);
    // __ movl(foundAt, r8);
    // __ bind(L_tail_8);
    
    __ addq(loopIndex, foundAt);
    __ movq(foundAt, loopIndex);

    return table_address;

    // if (result <= n - k) {
    //     return result;
    // } else {
    //     return std::string::npos;
    // }
    // __ bind(L_tail_3_9);
    // __ cmpq(foundAt, r9);
    // __ movq(rax, -1);
    // __ cmovq(Assembler::belowEqual, rax, foundAt);
}

address StubGenerator::generate_string_indexof() {
  StubCodeMark mark(this, "StubRoutines", "stringIndexOf");
  address jmp_table[13];
  int jmp_ndx = 0;
  __ align(CodeEntryAlignment);
  Label L_recurse;
  address start = __ pc();
  __ bind(L_recurse); // VP: No recursion in original c++.. (?)
  __ enter(); // required for proper stackwalking of RuntimeStub frame

  if (!VM_Version::supports_avx2()) {
    assert(false, "Only supports AVX2");
    return start;
  }

           // AVX2 version

    Label L_exit, L_anysize, L_outer_loop_guts, L_outer_loop, L_inner_loop, L_loop2;
    Label L_tail_10_12, L_tail_3_9, L_tail_1, L_tail_2, L_tail_3;
    Label L_str2_len_0, L_str2_len_1, L_str2_len_2, L_str2_len_3, L_str2_len_4;
    Label L_str2_len_5, L_str2_len_6, L_str2_len_7, L_str2_len_8, L_str2_len_9;
    Label L_str2_len_10, L_str2_len_11, L_str2_len_12;
    Label L_outer3, L_mid3, L_inner3, L_outer4, L_mid4, L_inner4;
    Label L_outer5, L_mid5, L_inner5, L_outer6, L_mid6, L_inner6;
    Label L_outer7, L_mid7, L_inner7, L_outer8, L_mid8, L_inner8;
    Label L_outer9, L_mid9, L_inner9, L_outer10, L_mid10, L_inner10;
    Label L_outer11, L_mid11, L_inner11, L_outer12, L_mid12, L_inner12;
    Label L_inner_mid11, L_inner_mid12,L_0x404f26, L_tail_8;
    Label L_inner1, L_outer1;

    Label L_begin, L_cross_page, L_no_recursion, L_pop_exit, L_byte_by_byte;
    Label L_bbb_outer, L_bbb_mid, L_bbb_found;

    Label strchr_avx2, memcmp_avx2;

    address jump_table;

    __ jmp(L_begin);

  { // CASE 0
    __ bind(L_str2_len_0);
    jmp_table[jmp_ndx++] = __ pc();
// jmp <_Z14avx2_strstr_v2PKcmS0_m+1525> L_exit
    __ xorq(rax, rax); // return 0
    __ jmp(L_exit);
  }
  
  { // avx2_strstr_anysize inlined
    __ bind(L_anysize);
// jmp <_Z14avx2_strstr_v2PKcmS0_m+156> L_outer_loop_guts
    __ vpbroadcastb(xmm0, Address(r11, 0, Address::times_1), Assembler::AVX_256bit);
    __ vmovdqu(Address(rsp, 0x40), xmm0);
    __ vpbroadcastb(xmm0, Address(r12, r11, Address::times_1, -1), Assembler::AVX_256bit);
    __ vmovdqu(Address(rsp, 0x20), xmm0);
    __ incrementq(r11);
    __ leaq(r13, Address(r12, -2));
    __ xorl(rax, rax);
    __ movq(Address(rsp, 0), r9);
    __ movq(Address(rsp, 0x18), rbx);
    __ subq(r10, rcx);
    __ incrementq(r10);
    __ movq(Address(rsp, 0x10), r10);
    __ jmpb(L_outer_loop_guts);

    __ bind(L_outer_loop);
// jae <_Z14avx2_strstr_v2PKcmS0_m+1511> L_tail_3_9
    __ movq(rax, Address(rsp, 8));
    __ addq(rax, 0x20);
    __ movq(rcx, -1);
    __ movq(r10, Address(rsp, 0x10));
    __ cmpq(rax, r10);
    __ movq(r9, Address(rsp, 0));
    __ movq(rbx, Address(rsp, 0x18));
    __ jae(L_tail_3_9);

    __ bind(L_outer_loop_guts);
// je <_Z14avx2_strstr_v2PKcmS0_m+117> L_outer_loop
    __ leaq(r14, Address(rbx, rax, Address::times_1));
    __ movq(Address(rsp, 0x8), rax);
    __ vmovdqu(xmm0, Address(rsp, 0x40));
    __ vpcmpeqb(xmm0, xmm0, Address(rbx, rax, Address::times_1), Assembler::AVX_256bit);
    __ vmovdqu(xmm1, Address(rsp, 0x20));
    __ vpcmpeqb(xmm1, xmm1, Address(r12, r14, Address::times_1, -1), Assembler::AVX_256bit);
    __ vpand(xmm0, xmm1, xmm0, Assembler::AVX_256bit);
    __ vpmovmskb(r15, xmm0, Assembler::AVX_256bit);
    __ testl(r15, r15);
    __ je_b(L_outer_loop);
    __ incrementq(r14);

    __ align(16);
    __ bind(L_inner_loop);
    //__ xorl(rbx, rbx);
    __ tzcntl(rbx, r15);
    __ leaq(rdi, Address(r14, rbx, Address::times_1));
    __ movq(rbp, r11);
    __ movq(rsi, r11);
    __ movq(rdx, r13);
    __ vzeroupper();
    __ call(memcmp_avx2, relocInfo::none);

// je <_Z14avx2_strstr_v2PKcmS0_m+1543> L_tail_1
// jne <_Z14avx2_strstr_v2PKcmS0_m+208> L_inner_loop
// jmp <_Z14avx2_strstr_v2PKcmS0_m+117> L_outer_loop
    __ testl(rax, rax);
    __ je(L_tail_1);
    __ blsrl(r15, r15);
    __ movq(r11, rbp);
    __ jne_b(L_inner_loop);
    __ jmp(L_outer_loop);
  } // end of avx2_strstr_anysize

  { // CASE 1
    __ bind(L_str2_len_1);
    jmp_table[jmp_ndx++] = __ pc();

    __ vpbroadcastb(xmm1, Address(r11, 0), Assembler::AVX_256bit);
    __ movq(rax, -1);
    __ xorl(r14, r14);
    __ jmpb(L_inner1);

    __ align(8);
    __ bind(L_outer1);
    __ addq(r14, 0x20);
    __ cmpq(r10, r14);
    __ jbe(L_exit); // rax already == -1

    __ bind(L_inner1);
    __ vpcmpeqb(xmm0, xmm1, Address(rbx, r14, Address::times_1), Assembler::AVX_256bit);
    __ vpmovmskb(r13, xmm0);
    __ testl(r13, r13);
    __ je_b(L_outer1);
    __ tzcntl(rax, r13);
    __ addq(rax, r14);

    __ cmpq(r9, rax);
    __ jae(L_exit);

    __ movq(rax, -1); // return -1
    __ jmp(L_exit);

  }

  { // CASE 2
    __ bind(L_str2_len_2);
    jmp_table[jmp_ndx++] = __ pc();
    __ vpbroadcastb(xmm0, Address(r11, 0, Address::times_1), Assembler::AVX_256bit);
    __ vpbroadcastb(xmm1, Address(r11, 1, Address::times_1), Assembler::AVX_256bit);
    __ vmovdqu(xmm2, Address(rbx, 0x0));
    __ movl(rax, 0x40);
    __ movq(rcx, -1);

    __ align(16);
    __ bind(L_loop2);
// jne <_Z14avx2_strstr_v2PKcmS0_m+1559> L_tail_2
    __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
    __ vextracti128(xmm4, xmm2, 0x1);
    __ vinserti128(xmm4, xmm4, Address(rbx, rax, Address::times_1, -0x20), 0x1);
    __ vpalignr(xmm2, xmm4, xmm2, 0x1, Assembler::AVX_256bit);
    __ vpcmpeqb(xmm2, xmm2, xmm1, Assembler::AVX_256bit);
    __ vpand(xmm2, xmm2, xmm3, Assembler::AVX_256bit);
    __ vpmovmskb(rsi, xmm2);
    __ testl(rsi, rsi);
    __ jne(L_tail_2);

// jae <_Z14avx2_strstr_v2PKcmS0_m+1511> L_tail_3_9
    __ leaq(rdx, Address(rax, -0x20));
    __ cmpq(rdx, r10);
    __ jae(L_tail_3_9);

// jne <_Z14avx2_strstr_v2PKcmS0_m+1566> L_tail_3
    __ vmovdqu(xmm2, Address(rbx, rax, Address::times_1, -0x20));
    __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
    __ vextracti128(xmm4, xmm2, 0x1);
    __ vinserti128(xmm4, xmm4, Address(rbx, rax, Address::times_1), 0x1);
    __ vpalignr(xmm2, xmm4, xmm2, 0x1, Assembler::AVX_256bit);
    __ vpcmpeqb(xmm2, xmm2, xmm1, Assembler::AVX_256bit);
    __ vpand(xmm2, xmm2, xmm3, Assembler::AVX_256bit);
    __ vpmovmskb(rsi, xmm2);
    __ testl(rsi, rsi);
    __ jne(L_tail_3);

// jae <_Z14avx2_strstr_v2PKcmS0_m+1511> L_tail_3_9
    __ cmpq(rax, r10);
    __ jae(L_tail_3_9);

// jmp <_Z14avx2_strstr_v2PKcmS0_m+336> L_loop2
    __ vmovdqu(xmm2, Address(rbx, rax, Address::times_1));
    __ addq(rax, 0x40);
    __ jmpb(L_loop2);
  }

  { // CASE 3
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 3, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 4
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 4, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 5
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 5, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 6
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 6, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 7
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 7, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 8
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 8, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 9
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 9, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 10
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 10, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }

  { // CASE 11
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 11, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  }
  
  { // CASE 12
    jmp_table[jmp_ndx++] = case_avx2_strstr(rbx, r10, r11, 12, L_tail_3_9, rax, 
                      rcx, rdx, rsi, rdi, r8, _masm);
  } // CASE 12

    // __ bind(L_tail_10_12); // swapped r8, no reg-spill here
    // __ movl(rcx, r8);

    // __ bind(L_tail_8); // Moved into each helper
    // __ addq(rax, rcx);
    // __ movq(rcx, rax);

    // if (result <= n - k) {
    //     return result;
    // } else {
    //     return std::string::npos; // VP: !!! The only way this is happening, read past allowed memory!
    // }
    __ bind(L_tail_3_9);
    __ cmpq(rcx, r9);
    __ movq(rax, -1);
    __ cmovq(Assembler::belowEqual, rax, rcx);

    __ bind(L_exit);
    __ addptr(rsp, 0x68);
#ifdef _WIN64
    __ pop(rdi);
    __ pop(rsi);
#endif
    __ pop(rbp);
    __ pop(rbx);
    __ pop(r12);
    __ pop(r13);
    __ pop(r14);
    __ pop(r15);
    __ vzeroupper();

    __ leave(); // required for proper stackwalking of RuntimeStub frame
    __ ret(0);

    __ bind(L_tail_1);
// jmp <_Z14avx2_strstr_v2PKcmS0_m+1511> L_tail_3_9
    __ movl(rax, rbx);
    __ movq(rcx, Address(rsp, 0x8));
    __ addq(rcx, rax);
    __ movq(r9, Address(rsp, 0));
    __ jmpb(L_tail_3_9);

    __ bind(L_tail_2);
    __ addq(rax, -64);
    __ movq(rdx, rax);

    __ bind(L_tail_3);
// jmp <_Z14avx2_strstr_v2PKcmS0_m+1511> L_tail_3_9
    //__ xorl(rcx, rcx);
    __ tzcntl(rcx, rsi);
    __ orq(rcx, rdx);
    __ jmpb(L_tail_3_9);

    __ align(8);

    jump_table = __ pc();

    for(jmp_ndx = 0; jmp_ndx < 13; jmp_ndx++) {
      __ emit_address(jmp_table[jmp_ndx]);
    }

    __ align(16);
    __ bind(L_begin);
// jb <_Z14avx2_strstr_v2PKcmS0_m+1525> L_exit
    __ push(r15);
    __ push(r14);
    __ push(r13);
    __ push(r12);
    __ push(rbx);
    __ push(rbp);
#ifdef _WIN64
    __ push(rsi);
    __ push(rdi);

    __ movq(rdi, rcx); // rdi = string
    __ movq(rsi, rdx); // rsi = len(string)
    __ movq(rdx, r8);  // rdx = needle
    __ movq(rcx, r9);  // rcx = len(needle)
#endif

    __ subptr(rsp, 0x68); // buy more stack (VP: find where used)
    __ xorq(rax, rax);    // rax = return value
    __ cmpq(rcx, rax);    // CASE 0: if (len(needle)==0) return 0 (VP: but also handled by jmp_table!)
    __ je(L_exit);        // VP: also(n<k) check is first? answer: can be moved("[0,)<0" always false)
    
    __ movq(rax, -1);
    __ movq(r9, rsi);
    __ subq(r9, rcx);
    __ jb(L_exit);         // if (len(string)<len(needle)) return -1

// ja <_Z14avx2_strstr_v2PKcmS0_m+67> L_anysize
    __ movq(r12, rcx);  // rcx = len(needle)
    __ movq(r11, rdx);  // rdx = needle
    __ movq(r10, rsi);  // rsi = len(string)
    __ movq(rbx, rdi);  // rdi = string

    // Check for potential page fault since we read 0x20 + needleSize
    // bytes beyond the end of the haystack.  If s[n] is within the
    // same page as s[n+k+0x20] then there's no chance of a page fault.
    // Equates to &s[n] & 0xfff < 0xfe0
    __ leaq(r14, Address(rbx, r10, Address::times_1, -0x1));  // &s[n-1] (VP: clever!!!)
    __ movq(r15, r14);
    __ andq(r15, 0xFFF);
    __ cmpq(r15, 0xFFF - 0x20);
    __ jg_b(L_cross_page);


    __ cmpq(rcx, 0xc);
    __ ja(L_anysize);  // CASE DEFAULT (len(needle>12))
    __ mov64(rax, (int64_t) jump_table);
    __ shlq(r12, 0x3);
    __ addq(rax, r12);  // rax = jump_table[sizeof(address)*len(needle)]
    __ shrq(r12, 0x3);  // Restore r12
    __ jmp(Address(rax, 0)); // CASE switch(len(needle)) (VP: neat! Could use Address(Address::times_8?)?)

    __ bind(L_cross_page); // VP: Alternatively, unroll last iteration? Never read any byte past strLen? i.e. for (size_t i = 0; i < n&31; i += 32) {
    //
    // Determine if any 32-byte chunks can be done
    __ movq(r14, rdi);
    __ push(rdi);   // Save initial string pointer
    __ subq(rsi, 0x20);
    __ jbe_b(L_no_recursion);

    __ push(rdi);
    __ push(rsi);
    __ push(rdx);
    __ push(rcx);
    __ call(L_recurse, relocInfo::none);
    __ pop(rcx);
    __ pop(rdx);
    __ pop(rsi);
    __ pop(rdi);
    __ cmpq(rax, -1);
    __ jne(L_pop_exit);

    __ addq(rdi, rsi);  // end-of-string - 0x20
    __ addq(rsi, 0x20);
    __ subq(rdi, rcx);  // subtract needle size
    __ cmpq(rdi, r14);
    __ cmovq(Assembler::belowEqual, rdi, r14);
    __ addq(r14, rsi);
    __ subq(r14, rcx);  // last byte to read
    __ movq(r13, rdi);  // first byte to read
    __ jmpb(L_byte_by_byte);

    __ bind(L_no_recursion);
    __ addq(rsi, 0x20);
    __ movq(r13, rdi);
    __ addq(r14, rsi);
    __ subq(r14, rcx);  // last byte to read

    __ bind(L_byte_by_byte);
    __ movq(r12, rdx);
    __ movq(r15, rcx);
    __ xorq(rbx, rbx);
    __ movq(rax, -1);
    __ load_unsigned_byte(r9, Address(r12, 0));  // first byte of needle
    __ jmpb(L_bbb_mid);

    __ bind(L_bbb_outer);
    __ addptr(r13, 1);
    __ cmpq(r13, r14);
    __ ja(L_pop_exit);

    __ bind(L_bbb_mid);
    __ cmpb(r9, Address(r13, 0));
    __ jne_b(L_bbb_outer);
    // First byte matches

    __ movq(rdi, r13);
    __ movq(rsi, r12);
    __ movq(rdx, r15);
    __ call(memcmp_avx2, relocInfo::none);
    __ testl(rax, rax);
    __ je_b(L_bbb_found);
    __ movq(rax, -1);
    __ jmpb(L_bbb_outer);

    __ bind(L_bbb_found);
    __ pop(rax);        // get back pointer to start of string
    __ subq(r13, rax);
    __ movq(rax, r13);
    __ jmp(L_exit);

    __ bind(L_pop_exit);
    __ pop(r15);
    __ jmp(L_exit);

  __ align(CodeEntryAlignment);
    __ bind(memcmp_avx2);

//    1 /* memcmp/wmemcmp optimized with AVX2.
//    2    Copyright (C) 2017-2023 Free Software Foundation, Inc.
//    3    This file is part of the GNU C Library.
//    4
//    5    The GNU C Library is free software; you can redistribute it and/or
//    6    modify it under the terms of the GNU Lesser General Public
//    7    License as published by the Free Software Foundation; either
//    8    version 2.1 of the License, or (at your option) any later version.
//    9
//   10    The GNU C Library is distributed in the hope that it will be useful,
//   11    but WITHOUT ANY WARRANTY; without even the implied warranty of
//   12    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   13    Lesser General Public License for more details.
//   14
//   15    You should have received a copy of the GNU Lesser General Public
//   16    License along with the GNU C Library; if not, see
//   17    <https://www.gnu.org/licenses/>.  */
//   18
//   23 /* memcmp/wmemcmp is implemented as:
//   24    1. Use ymm vector compares when possible. The only case where
//   25       vector compares is not possible for when size < VEC_SIZE
//   26       and loading from either s1 or s2 would cause a page cross.
//   27    2. For size from 2 to 7 bytes on page cross, load as big endian
//   28       with movbe and bswap to avoid branches.
//   29    3. Use xmm vector compare when size >= 4 bytes for memcmp or
//   30       size >= 8 bytes for wmemcmp.
//   31    4. Optimistically compare up to first 4 * VEC_SIZE one at a
//   32       to check for early mismatches. Only do this if its guaranteed the
//   33       work is not wasted.
//   34    5. If size is 8 * VEC_SIZE or less, unroll the loop.
//   35    6. Compare 4 * VEC_SIZE at a time with the aligned first memory
//   36       area.
//   37    7. Use 2 vector compares when size is 2 * VEC_SIZE or less.
//   38    8. Use 4 vector compares when size is 4 * VEC_SIZE or less.
//   39    9. Use 8 vector compares when size is 8 * VEC_SIZE or less.  */

    Label L_less_vec, L_return_vec_0, L_last_1x_vec, L_return_vec_1, L_last_2x_vec;
    Label L_return_vec_2, L_retun_vec_3, L_more_8x_vec, L_return_vec_0_1_2_3;
    Label L_return_vzeroupper, L_8x_return_vec_0_1_2_3, L_loop_4x_vec;
    Label L_return_vec_3, L_8x_last_1x_vec, L_8x_last_2x_vec, L_8x_return_vec_2;
    Label L_8x_return_vec_3, L_return_vec_1_end, L_return_vec_0_end, L_one_or_less;
    Label L_page_cross_less_vec, L_between_16_31, L_between_8_15, L_between_2_3, L_zero;
    Label L_ret_nonzero;

// Dump of assembler code for function __memcmp_avx2_movbe:
// 71

// 72              .section SECTION(.text),"ax",@progbits
// 73      ENTRY (MEMCMP)
// 74      # ifdef USE_AS_WMEMCMP
// 75              shl     $2, %RDX_LP
// 76      # elif defined __ILP32__
// 77              /* Clear the upper 32 bits.  */
// 78              movl    %edx, %edx
    __ cmpq(rdx, 0x20);

// 79      # endif
    __ jb(L_less_vec);

// 80              cmp     $VEC_SIZE, %RDX_LP
// 81              jb      L(less_vec)
// 82
    __ vmovdqu(xmm1, Address(rsi, 0));

// 83              /* From VEC to 2 * VEC.  No branch when size == VEC_SIZE.  */
    __ vpcmpeqb(xmm1, xmm1, Address(rdi, 0), Assembler::AVX_256bit);

// 84              vmovdqu (%rsi), %ymm1
    __ vpmovmskb(rax, xmm1, Assembler::AVX_256bit);

// 85              VPCMPEQ (%rdi), %ymm1, %ymm1
// 86              vpmovmskb %ymm1, %eax
// 87              /* NB: eax must be destination register if going to
// 88                 L(return_vec_[0,2]). For L(return_vec_3 destination register
    __ incrementl(rax);

// 89                 must be ecx.  */
    __ jne(L_return_vec_0);

// 90              incl    %eax
// 91              jnz     L(return_vec_0)
    __ cmpq(rdx, 0x40);

// 92
    __ jbe(L_last_1x_vec);

// 93              cmpq    $(VEC_SIZE * 2), %rdx
// 94              jbe     L(last_1x_vec)
// 95
    __ vmovdqu(xmm2, Address(rsi, 0x20));

// 96              /* Check second VEC no matter what.  */
    __ vpcmpeqb(xmm2, xmm2, Address(rdi, 0x20), Assembler::AVX_256bit);

// 97              vmovdqu VEC_SIZE(%rsi), %ymm2
    __ vpmovmskb(rax, xmm2, Assembler::AVX_256bit);

// 98              VPCMPEQ VEC_SIZE(%rdi), %ymm2, %ymm2
// 99              vpmovmskb %ymm2, %eax
// 100             /* If all 4 VEC where equal eax will be all 1s so incl will
    __ incrementl(rax);

// 101                overflow and set zero flag.  */
    __ jne(L_return_vec_1);

// 102             incl    %eax
// 103             jnz     L(return_vec_1)
// 104
    __ cmpq(rdx, 0x80);

// 105             /* Less than 4 * VEC.  */
    __ jbe(L_last_2x_vec);

// 106             cmpq    $(VEC_SIZE * 4), %rdx
// 107             jbe     L(last_2x_vec)
// 108
    __ vmovdqu(xmm3, Address(rsi, 0x40));

// 109             /* Check third and fourth VEC no matter what.  */
    __ vpcmpeqb(xmm3, xmm3, Address(rdi, 0x40), Assembler::AVX_256bit);

// 110             vmovdqu (VEC_SIZE * 2)(%rsi), %ymm3
    __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);

// 111             VPCMPEQ (VEC_SIZE * 2)(%rdi), %ymm3, %ymm3
    __ incrementl(rax);

// 112             vpmovmskb %ymm3, %eax
    __ jne(L_return_vec_2);

// 113             incl    %eax
    __ vmovdqu(xmm4, Address(rsi, 0x60));

// 114             jnz     L(return_vec_2)
    __ vpcmpeqb(xmm4, xmm4, Address(rdi, 0x60), Assembler::AVX_256bit);

// 115             vmovdqu (VEC_SIZE * 3)(%rsi), %ymm4
    __ vpmovmskb(rcx, xmm4, Assembler::AVX_256bit);

// 116             VPCMPEQ (VEC_SIZE * 3)(%rdi), %ymm4, %ymm4
    __ incrementl(rcx);

// 117             vpmovmskb %ymm4, %ecx
    __ jne(L_return_vec_3);

// 118             incl    %ecx
// 119             jnz     L(return_vec_3)
// 120
    __ cmpq(rdx, 0x100);

// 121             /* Go to 4x VEC loop.  */
    __ ja(L_more_8x_vec);

// 122             cmpq    $(VEC_SIZE * 8), %rdx
// 123             ja      L(more_8x_vec)
// 124
// 125             /* Handle remainder of size = 4 * VEC + 1 to 8 * VEC without any
// 126                branches.  */
// 127
    __ vmovdqu(xmm1, Address(rsi, rdx, Address::times_1, -0x80));

// 128             /* Load first two VEC from s2 before adjusting addresses.  */
    __ vmovdqu(xmm2, Address(rsi, rdx, Address::times_1, -0x60));

// 129             vmovdqu -(VEC_SIZE * 4)(%rsi, %rdx), %ymm1
    __ leaq(rdi, Address(rdi, rdx, Address::times_1, -0x80));

// 130             vmovdqu -(VEC_SIZE * 3)(%rsi, %rdx), %ymm2
    __ leaq(rsi, Address(rsi, rdx, Address::times_1, -0x80));

// 131             leaq    -(4 * VEC_SIZE)(%rdi, %rdx), %rdi
// 132             leaq    -(4 * VEC_SIZE)(%rsi, %rdx), %rsi
// 133
// 134             /* Wait to load from s1 until addressed adjust due to
    __ vpcmpeqb(xmm1, xmm1, Address(rdi, 0), Assembler::AVX_256bit);

// 135                unlamination of microfusion with complex address mode.  */
    __ vpcmpeqb(xmm2, xmm2, Address(rdi, 0x20), Assembler::AVX_256bit);

// 136             VPCMPEQ (%rdi), %ymm1, %ymm1
// 137             VPCMPEQ (VEC_SIZE)(%rdi), %ymm2, %ymm2
    __ vmovdqu(xmm3, Address(rsi, 0x40));

// 138
    __ vpcmpeqb(xmm3, xmm3, Address(rdi, 0x40), Assembler::AVX_256bit);

// 139             vmovdqu (VEC_SIZE * 2)(%rsi), %ymm3
    __ vmovdqu(xmm4, Address(rsi, 0x60));

// 140             VPCMPEQ (VEC_SIZE * 2)(%rdi), %ymm3, %ymm3
    __ vpcmpeqb(xmm4, xmm4, Address(rdi, 0x60), Assembler::AVX_256bit);

// 141             vmovdqu (VEC_SIZE * 3)(%rsi), %ymm4
// 142             VPCMPEQ (VEC_SIZE * 3)(%rdi), %ymm4, %ymm4
// 143
    __ vpand(xmm5, xmm2, xmm1, Assembler::AVX_256bit);

// 144             /* Reduce VEC0 - VEC4.  */
    __ vpand(xmm6, xmm4, xmm3, Assembler::AVX_256bit);

// 145             vpand   %ymm1, %ymm2, %ymm5
    __ vpand(xmm7, xmm6, xmm5, Assembler::AVX_256bit);

// 146             vpand   %ymm3, %ymm4, %ymm6
    __ vpmovmskb(rcx, xmm7, Assembler::AVX_256bit);

// 147             vpand   %ymm5, %ymm6, %ymm7
    __ incrementl(rcx);

// 148             vpmovmskb %ymm7, %ecx
    __ jne_b(L_return_vec_0_1_2_3);

// 149             incl    %ecx
// 150             jnz     L(return_vec_0_1_2_3)
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_return_vec_0);

// 151             /* NB: eax must be zero to reach here.  */
// 152             VZEROUPPER_RETURN
// 153
// 154             .p2align 4
    __ tzcntl(rax, rax);

// 155     L(return_vec_0):
// 156             tzcntl  %eax, %eax
// 157     # ifdef USE_AS_WMEMCMP
// 158             movl    (%rdi, %rax), %ecx
// 159             xorl    %edx, %edx
// 160             cmpl    (%rsi, %rax), %ecx
// 161             /* NB: no partial register stall here because xorl zero idiom
// 162                above.  */
// 163             setg    %dl
// 164             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rcx, Address(rsi, rax, Address::times_1));

// 165     # else
    __ movzbl(rax, Address(rdi, rax, Address::times_1));

// 166             movzbl  (%rsi, %rax), %ecx
    __ subl(rax, rcx);

// 167             movzbl  (%rdi, %rax), %eax
// 168             subl    %ecx, %eax
// 169     # endif
    __ bind(L_return_vzeroupper);
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_return_vec_1);

// 170     L(return_vzeroupper):
// 171             ZERO_UPPER_VEC_REGISTERS_RETURN
// 172
// 173             .p2align 4
    __ tzcntl(rax, rax);

// 174     L(return_vec_1):
// 175             tzcntl  %eax, %eax
// 176     # ifdef USE_AS_WMEMCMP
// 177             movl    VEC_SIZE(%rdi, %rax), %ecx
// 178             xorl    %edx, %edx
// 179             cmpl    VEC_SIZE(%rsi, %rax), %ecx
// 180             setg    %dl
// 181             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rcx, Address(rsi, rax, Address::times_1, 0x20));

// 182     # else
    __ movzbl(rax, Address(rdi, rax, Address::times_1, 0x20));

// 183             movzbl  VEC_SIZE(%rsi, %rax), %ecx
    __ subl(rax, rcx);

// 184             movzbl  VEC_SIZE(%rdi, %rax), %eax
// 185             subl    %ecx, %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_return_vec_2);

// 186     # endif
// 187             VZEROUPPER_RETURN
// 188
// 189             .p2align 4
    __ tzcntl(rax, rax);

// 190     L(return_vec_2):
// 191             tzcntl  %eax, %eax
// 192     # ifdef USE_AS_WMEMCMP
// 193             movl    (VEC_SIZE * 2)(%rdi, %rax), %ecx
// 194             xorl    %edx, %edx
// 195             cmpl    (VEC_SIZE * 2)(%rsi, %rax), %ecx
// 196             setg    %dl
// 197             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rcx, Address(rsi, rax, Address::times_1, 0x40));

// 198     # else
    __ movzbl(rax, Address(rdi, rax, Address::times_1, 0x40));

// 199             movzbl  (VEC_SIZE * 2)(%rsi, %rax), %ecx
    __ subl(rax, rcx);

// 200             movzbl  (VEC_SIZE * 2)(%rdi, %rax), %eax
// 201             subl    %ecx, %eax
    __ vzeroupper();
    __ ret(0);
    __ align(32);

    __ bind(L_8x_return_vec_0_1_2_3);

// 202     # endif
// 203             VZEROUPPER_RETURN
// 204
// 205             /* NB: p2align 5 here to ensure 4x loop is 32 byte aligned.  */
// 206             .p2align 5
// 207     L(8x_return_vec_0_1_2_3):
    __ addq(rsi, rdi);

// 208             /* Returning from L(more_8x_vec) requires restoring rsi.  */
// 209             addq    %rdi, %rsi
    __ bind(L_return_vec_0_1_2_3);
    __ vpmovmskb(rax, xmm1, Assembler::AVX_256bit);

// 210     L(return_vec_0_1_2_3):
    __ incrementl(rax);

// 211             vpmovmskb %ymm1, %eax
    __ jne_b(L_return_vec_0);

// 212             incl    %eax
// 213             jnz     L(return_vec_0)
    __ vpmovmskb(rax, xmm2, Assembler::AVX_256bit);

// 214
    __ incrementl(rax);

// 215             vpmovmskb %ymm2, %eax
    __ jne_b(L_return_vec_1);

// 216             incl    %eax
// 217             jnz     L(return_vec_1)
    __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);

// 218
    __ incrementl(rax);

// 219             vpmovmskb %ymm3, %eax
    __ jne_b(L_return_vec_2);

// 220             incl    %eax
// 221             jnz     L(return_vec_2)
    __ bind(L_return_vec_3);
    __ tzcntl(rcx, rcx);

// 222     L(return_vec_3):
// 223             tzcntl  %ecx, %ecx
// 224     # ifdef USE_AS_WMEMCMP
// 225             movl    (VEC_SIZE * 3)(%rdi, %rcx), %eax
// 226             xorl    %edx, %edx
// 227             cmpl    (VEC_SIZE * 3)(%rsi, %rcx), %eax
// 228             setg    %dl
// 229             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rax, Address(rdi, rcx, Address::times_1, 0x60));

// 230     # else
    __ movzbl(rcx, Address(rsi, rcx, Address::times_1, 0x60));

// 231             movzbl  (VEC_SIZE * 3)(%rdi, %rcx), %eax
    __ subl(rax, rcx);

// 232             movzbl  (VEC_SIZE * 3)(%rsi, %rcx), %ecx
// 233             subl    %ecx, %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_more_8x_vec);

// 234     # endif
// 235             VZEROUPPER_RETURN
// 236
// 237             .p2align 4
// 238     L(more_8x_vec):
    __ leaq(rdx, Address(rdi, rdx, Address::times_1, -0x80));

// 239             /* Set end of s1 in rdx.  */
// 240             leaq    -(VEC_SIZE * 4)(%rdi, %rdx), %rdx
// 241             /* rsi stores s2 - s1. This allows loop to only update one
    __ subq(rsi, rdi);

// 242                pointer.  */
// 243             subq    %rdi, %rsi
    __ andq(rdi, -32);

// 244             /* Align s1 pointer.  */
// 245             andq    $-VEC_SIZE, %rdi
    __ subq(rdi, -128);
    __ align(16);
    __ bind(L_loop_4x_vec);

// 246             /* Adjust because first 4x vec where check already.  */
// 247             subq    $-(VEC_SIZE * 4), %rdi
// 248             .p2align 4
// 249     L(loop_4x_vec):
// 250             /* rsi has s2 - s1 so get correct address by adding s1 (in rdi).
    __ vmovdqu(xmm1, Address(rsi, rdi, Address::times_1));

// 251              */
    __ vpcmpeqb(xmm1, xmm1, Address(rdi, 0), Assembler::AVX_256bit);

// 252             vmovdqu (%rsi, %rdi), %ymm1
// 253             VPCMPEQ (%rdi), %ymm1, %ymm1
    __ vmovdqu(xmm2, Address(rsi, rdi, Address::times_1, 0x20));

// 254
    __ vpcmpeqb(xmm2, xmm2, Address(rdi, 0x20), Assembler::AVX_256bit);

// 255             vmovdqu VEC_SIZE(%rsi, %rdi), %ymm2
// 256             VPCMPEQ VEC_SIZE(%rdi), %ymm2, %ymm2
    __ vmovdqu(xmm3, Address(rsi, rdi, Address::times_1, 0x40));

// 257
    __ vpcmpeqb(xmm3, xmm3, Address(rdi, 0x40), Assembler::AVX_256bit);

// 258             vmovdqu (VEC_SIZE * 2)(%rsi, %rdi), %ymm3
// 259             VPCMPEQ (VEC_SIZE * 2)(%rdi), %ymm3, %ymm3
    __ vmovdqu(xmm4, Address(rsi, rdi, Address::times_1, 0x60));

// 260
    __ vpcmpeqb(xmm4, xmm4, Address(rdi, 0x60), Assembler::AVX_256bit);

// 261             vmovdqu (VEC_SIZE * 3)(%rsi, %rdi), %ymm4
// 262             VPCMPEQ (VEC_SIZE * 3)(%rdi), %ymm4, %ymm4
    __ vpand(xmm5, xmm2, xmm1, Assembler::AVX_256bit);

// 263
    __ vpand(xmm6, xmm4, xmm3, Assembler::AVX_256bit);

// 264             vpand   %ymm1, %ymm2, %ymm5
    __ vpand(xmm7, xmm6, xmm5, Assembler::AVX_256bit);

// 265             vpand   %ymm3, %ymm4, %ymm6
    __ vpmovmskb(rcx, xmm7, Assembler::AVX_256bit);

// 266             vpand   %ymm5, %ymm6, %ymm7
    __ incrementl(rcx);

// 267             vpmovmskb %ymm7, %ecx
    __ jne_b(L_8x_return_vec_0_1_2_3);

// 268             incl    %ecx
    __ subq(rdi, -128);

// 269             jnz     L(8x_return_vec_0_1_2_3)
// 270             subq    $-(VEC_SIZE * 4), %rdi
    __ cmpq(rdi, rdx);

// 271             /* Check if s1 pointer at end.  */
    __ jb_b(L_loop_4x_vec);

// 272             cmpq    %rdx, %rdi
// 273             jb      L(loop_4x_vec)
    __ subq(rdi, rdx);

// 274
// 275             subq    %rdx, %rdi
    __ cmpl(rdi, 0x60);

// 276             /* rdi has 4 * VEC_SIZE - remaining length.  */
    __ jae_b(L_8x_last_1x_vec);

// 277             cmpl    $(VEC_SIZE * 3), %edi
// 278             jae     L(8x_last_1x_vec)
    __ vmovdqu(xmm3, Address(rsi, rdx, Address::times_1, 0x40));

// 279             /* Load regardless of branch.  */
    __ cmpl(rdi, 0x40);

// 280             vmovdqu (VEC_SIZE * 2)(%rsi, %rdx), %ymm3
    __ jae_b(L_8x_last_2x_vec);

// 281             cmpl    $(VEC_SIZE * 2), %edi
// 282             jae     L(8x_last_2x_vec)
// 283
    __ vmovdqu(xmm1, Address(rsi, rdx, Address::times_1));

// 284             /* Check last 4 VEC.  */
    __ vpcmpeqb(xmm1, xmm1, Address(rdx, 0), Assembler::AVX_256bit);

// 285             vmovdqu (%rsi, %rdx), %ymm1
// 286             VPCMPEQ (%rdx), %ymm1, %ymm1
    __ vmovdqu(xmm2, Address(rsi, rdx, Address::times_1, 0x20));

// 287
    __ vpcmpeqb(xmm2, xmm2, Address(rdx, 0x20), Assembler::AVX_256bit);

// 288             vmovdqu VEC_SIZE(%rsi, %rdx), %ymm2
// 289             VPCMPEQ VEC_SIZE(%rdx), %ymm2, %ymm2
    __ vpcmpeqb(xmm3, xmm3, Address(rdx, 0x40), Assembler::AVX_256bit);

// 290
// 291             VPCMPEQ (VEC_SIZE * 2)(%rdx), %ymm3, %ymm3
    __ vmovdqu(xmm4, Address(rsi, rdx, Address::times_1, 0x60));

// 292
    __ vpcmpeqb(xmm4, xmm4, Address(rdx, 0x60), Assembler::AVX_256bit);

// 293             vmovdqu (VEC_SIZE * 3)(%rsi, %rdx), %ymm4
// 294             VPCMPEQ (VEC_SIZE * 3)(%rdx), %ymm4, %ymm4
    __ vpand(xmm5, xmm2, xmm1, Assembler::AVX_256bit);

// 295
    __ vpand(xmm6, xmm4, xmm3, Assembler::AVX_256bit);

// 296             vpand   %ymm1, %ymm2, %ymm5
    __ vpand(xmm7, xmm6, xmm5, Assembler::AVX_256bit);

// 297             vpand   %ymm3, %ymm4, %ymm6
    __ vpmovmskb(rcx, xmm7, Assembler::AVX_256bit);

// 298             vpand   %ymm5, %ymm6, %ymm7
// 299             vpmovmskb %ymm7, %ecx
    __ movq(rdi, rdx);

// 300             /* Restore s1 pointer to rdi.  */
    __ incrementl(rcx);

// 301             movq    %rdx, %rdi
    __ jne(L_8x_return_vec_0_1_2_3);

// 302             incl    %ecx
// 303             jnz     L(8x_return_vec_0_1_2_3)
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_8x_last_2x_vec);

// 304             /* NB: eax must be zero to reach here.  */
// 305             VZEROUPPER_RETURN
// 306
// 307             /* Only entry is from L(more_8x_vec).  */
// 308             .p2align 4
// 309     L(8x_last_2x_vec):
// 310             /* Check second to last VEC. rdx store end pointer of s1 and
// 311                ymm3 has already been loaded with second to last VEC from s2.
    __ vpcmpeqb(xmm3, xmm3, Address(rdx, 0x40), Assembler::AVX_256bit);

// 312              */
    __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);

// 313             VPCMPEQ (VEC_SIZE * 2)(%rdx), %ymm3, %ymm3
    __ incrementl(rax);

// 314             vpmovmskb %ymm3, %eax
    __ jne_b(L_8x_return_vec_2);
    __ align(16);

    __ bind(L_8x_last_1x_vec);

// 315             incl    %eax
// 316             jnz     L(8x_return_vec_2)
// 317             /* Check last VEC.  */
// 318             .p2align 4
    __ vmovdqu(xmm4, Address(rsi, rdx, Address::times_1, 0x60));

// 319     L(8x_last_1x_vec):
    __ vpcmpeqb(xmm4, xmm4, Address(rdx, 0x60), Assembler::AVX_256bit);

// 320             vmovdqu (VEC_SIZE * 3)(%rsi, %rdx), %ymm4
    __ vpmovmskb(rax, xmm4, Assembler::AVX_256bit);

// 321             VPCMPEQ (VEC_SIZE * 3)(%rdx), %ymm4, %ymm4
    __ incrementl(rax);

// 322             vpmovmskb %ymm4, %eax
    __ jne_b(L_8x_return_vec_3);

// 323             incl    %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_last_2x_vec);

// 324             jnz     L(8x_return_vec_3)
// 325             VZEROUPPER_RETURN
// 326
// 327             .p2align 4
// 328     L(last_2x_vec):
    __ vmovdqu(xmm1, Address(rsi, rdx, Address::times_1, -0x40));

// 329             /* Check second to last VEC.  */
    __ vpcmpeqb(xmm1, xmm1, Address(rdi, rdx, Address::times_1, -0x40), Assembler::AVX_256bit);

// 330             vmovdqu -(VEC_SIZE * 2)(%rsi, %rdx), %ymm1
    __ vpmovmskb(rax, xmm1, Assembler::AVX_256bit);

// 331             VPCMPEQ -(VEC_SIZE * 2)(%rdi, %rdx), %ymm1, %ymm1
    __ incrementl(rax);

// 332             vpmovmskb %ymm1, %eax
    __ jne_b(L_return_vec_1_end);

    __ bind(L_last_1x_vec);

// 333             incl    %eax
// 334             jnz     L(return_vec_1_end)
// 335             /* Check last VEC.  */
    __ vmovdqu(xmm1, Address(rsi, rdx, Address::times_1, -0x20));

// 336     L(last_1x_vec):
    __ vpcmpeqb(xmm1, xmm1, Address(rdi, rdx, Address::times_1, -0x20), Assembler::AVX_256bit);

// 337             vmovdqu -(VEC_SIZE * 1)(%rsi, %rdx), %ymm1
    __ vpmovmskb(rax, xmm1, Assembler::AVX_256bit);

// 338             VPCMPEQ -(VEC_SIZE * 1)(%rdi, %rdx), %ymm1, %ymm1
    __ incrementl(rax);

// 339             vpmovmskb %ymm1, %eax
    __ jne_b(L_return_vec_0_end);

// 340             incl    %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_8x_return_vec_2);

// 341             jnz     L(return_vec_0_end)
// 342             VZEROUPPER_RETURN
// 343
// 344             .p2align 4
    __ subq(rdx, 0x20);
    __ bind(L_8x_return_vec_3);

// 345     L(8x_return_vec_2):
// 346             subq    $VEC_SIZE, %rdx
    __ tzcntl(rax, rax);

// 347     L(8x_return_vec_3):
    __ addq(rax, rdx);

// 348             tzcntl  %eax, %eax
// 349             addq    %rdx, %rax
// 350     # ifdef USE_AS_WMEMCMP
// 351             movl    (VEC_SIZE * 3)(%rax), %ecx
// 352             xorl    %edx, %edx
// 353             cmpl    (VEC_SIZE * 3)(%rsi, %rax), %ecx
// 354             setg    %dl
// 355             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rcx, Address(rsi, rax, Address::times_1, 0x60));

// 356     # else
    __ movzbl(rax, Address(rax, 0x60));

// 357             movzbl  (VEC_SIZE * 3)(%rsi, %rax), %ecx
    __ subl(rax, rcx);

// 358             movzbl  (VEC_SIZE * 3)(%rax), %eax
// 359             subl    %ecx, %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_return_vec_1_end);

// 360     # endif
// 361             VZEROUPPER_RETURN
// 362
// 363             .p2align 4
    __ tzcntl(rax, rax);

// 364     L(return_vec_1_end):
    __ addl(rax, rdx);

// 365             tzcntl  %eax, %eax
// 366             addl    %edx, %eax
// 367     # ifdef USE_AS_WMEMCMP
// 368             movl    -(VEC_SIZE * 2)(%rdi, %rax), %ecx
// 369             xorl    %edx, %edx
// 370             cmpl    -(VEC_SIZE * 2)(%rsi, %rax), %ecx
// 371             setg    %dl
// 372             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rcx, Address(rsi, rax, Address::times_1, -0x40));

// 373     # else
    __ movzbl(rax, Address(rdi, rax, Address::times_1, -0x40));

// 374             movzbl  -(VEC_SIZE * 2)(%rsi, %rax), %ecx
    __ subl(rax, rcx);

// 375             movzbl  -(VEC_SIZE * 2)(%rdi, %rax), %eax
// 376             subl    %ecx, %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_return_vec_0_end);

// 377     # endif
// 378             VZEROUPPER_RETURN
// 379
// 380             .p2align 4
    __ tzcntl(rax, rax);

// 381     L(return_vec_0_end):
    __ addl(rax, rdx);

// 382             tzcntl  %eax, %eax
// 383             addl    %edx, %eax
// 384     # ifdef USE_AS_WMEMCMP
// 385             movl    -VEC_SIZE(%rdi, %rax), %ecx
// 386             xorl    %edx, %edx
// 387             cmpl    -VEC_SIZE(%rsi, %rax), %ecx
// 388             setg    %dl
// 389             leal    -1(%rdx, %rdx), %eax
    __ movzbl(rcx, Address(rsi, rax, Address::times_1, -0x20));

// 390     # else
    __ movzbl(rax, Address(rdi, rax, Address::times_1, -0x20));

// 391             movzbl  -VEC_SIZE(%rsi, %rax), %ecx
    __ subl(rax, rcx);

// 392             movzbl  -VEC_SIZE(%rdi, %rax), %eax
// 393             subl    %ecx, %eax
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_less_vec);

// 394     # endif
// 395             VZEROUPPER_RETURN
// 396
// 397             .p2align 4
// 398     L(less_vec):
// 399             /* Check if one or less CHAR. This is necessary for size = 0 but
    __ cmpl(rdx, 0x1);

// 400                is also faster for size = CHAR_SIZE.  */
    __ jbe_b(L_one_or_less);

// 401             cmpl    $CHAR_SIZE, %edx
// 402             jbe     L(one_or_less)
// 403
// 404             /* Check if loading one VEC from either s1 or s2 could cause a
// 405                page cross. This can have false positives but is by far the
    __ movl(rax, rdi);

// 406                fastest method.  */
    __ orl(rax, rsi);

// 407             movl    %edi, %eax
    __ andl(rax, 0xfff);

// 408             orl     %esi, %eax
    __ cmpl(rax, 0xfe0);

// 409             andl    $(PAGE_SIZE - 1), %eax
    __ jg_b(L_page_cross_less_vec);

// 410             cmpl    $(PAGE_SIZE - VEC_SIZE), %eax
// 411             jg      L(page_cross_less_vec)
// 412
    __ vmovdqu(xmm2, Address(rsi, 0));

// 413             /* No page cross possible.  */
    __ vpcmpeqb(xmm2, xmm2, Address(rdi, 0), Assembler::AVX_256bit);

// 414             vmovdqu (%rsi), %ymm2
    __ vpmovmskb(rax, xmm2, Assembler::AVX_256bit);

// 415             VPCMPEQ (%rdi), %ymm2, %ymm2
    __ incrementl(rax);

// 416             vpmovmskb %ymm2, %eax
// 417             incl    %eax
// 418             /* Result will be zero if s1 and s2 match. Otherwise first set
    __ bzhil(rdx, rax, rdx);

// 419                bit will be first mismatch.  */
    __ jne(L_return_vec_0);

// 420             bzhil   %edx, %eax, %edx
    __ xorl(rax, rax);

// 421             jnz     L(return_vec_0)
    __ vzeroupper();
    __ ret(0);
    __ align(16);

    __ bind(L_page_cross_less_vec);

// 422             xorl    %eax, %eax
// 423             VZEROUPPER_RETURN
// 424
// 425             .p2align 4
// 426     L(page_cross_less_vec):
// 427             /* if USE_AS_WMEMCMP it can only be 0, 4, 8, 12, 16, 20, 24, 28
    __ cmpl(rdx, 0x10);

// 428                bytes.  */
    __ jae(L_between_16_31);

// 429             cmpl    $16, %edx
// 430             jae     L(between_16_31)
    __ cmpl(rdx, 0x8);

// 431     # ifndef USE_AS_WMEMCMP
    __ jae_b(L_between_8_15);

// 432             cmpl    $8, %edx
    __ cmpl(rdx, 0x4);

// 433             jae     L(between_8_15)
    __ jae(L_between_2_3);

// 434             /* Fall through for [4, 7].  */
// 435             cmpl    $4, %edx
// 436             jb      L(between_2_3)
    __ movzbl(rax, Address(rdi, 0));

// 437
    __ movzbl(rcx, Address(rsi, 0));

// 438             movbe   (%rdi), %eax
// 439             movbe   (%rsi), %ecx

//  33b:	48 c1 e0 20          	shl    rax,0x20
   __ shlq(rax, 0x20);
// 	shlq	$32, %rcx
//  33f:	48 c1 e1 20          	shl    rcx,0x20
    __ shlq(rcx, 0x20);
// 	movbe	-4(%rdi, %rdx), %edi
//  343:	0f 38 f0 7c 17 fc    	movbe  edi,DWORD PTR [rdi+rdx*1-0x4]
    __ movzbl(rdi, Address(rdi, rdx, Address::times_1, -0x4));
// 	movbe	-4(%rsi, %rdx), %esi
//  349:	0f 38 f0 74 16 fc    	movbe  esi,DWORD PTR [rsi+rdx*1-0x4]
    __ movzbl(rsi, Address(rsi, rdx, Address::times_1, -0x4));
// 	orq	%rdi, %rax
//  34f:	48 09 f8             	or     rax,rdi
    __ orq(rax, rdi);
// 	orq	%rsi, %rcx
//  352:	48 09 f1             	or     rcx,rsi
    __ orq(rcx, rsi);
// 	subq	%rcx, %rax
//  355:	48 29 c8             	sub    rax,rcx
    __ subq(rax, rcx);
// 	/* Fast path for return zero.  */
// 	jnz	L(ret_nonzero)
//  358:	75 16                	jne    370 <__memcmp_avx2_movbe+0x370>
    __ jne_b(L_ret_nonzero);
// 	/* No ymm register was touched.  */
// 	ret
//  35a:	c3                   	ret
//  35b:	0f 1f 44 00 00       	nop    DWORD PTR [rax+rax*1+0x0]
    __ ret(0);
    __ align(16);

    __ bind(L_one_or_less);

// 	.p2align 4
// L(one_or_less):
// 	jb	L(zero)
//  360:	72 14                	jb     376 <__memcmp_avx2_movbe+0x376>
    __ jb_b(L_zero);
// 	movzbl	(%rsi), %ecx
//  362:	0f b6 0e             	movzx  ecx,BYTE PTR [rsi]
    __ movzbl(rcx, Address(rsi, 0));
// 	movzbl	(%rdi), %eax
//  365:	0f b6 07             	movzx  eax,BYTE PTR [rdi]
    __ movzbl(rax, Address(rdi, 0));
// 	subl	%ecx, %eax
//  368:	29 c8                	sub    eax,ecx
    __ subl(rax, rcx);
// 	/* No ymm register was touched.  */
// 	ret
//  36a:	c3                   	ret
//  36b:	0f 1f 44 00 00       	nop    DWORD PTR [rax+rax*1+0x0]
    __ ret(0);
    __ p2align(16, 5);

    __ bind(L_ret_nonzero);

// 	.p2align 4,, 5
// L(ret_nonzero):
// 	sbbl	%eax, %eax
//  370:	19 c0                	sbb    eax,eax
    __ sbbl(rax, rax);
// 	orl	$1, %eax
//  372:	83 c8 01             	or     eax,0x1
    __ orl(rax, 0x1);
// 	/* No ymm register was touched.  */
// 	ret
//  375:	c3                   	ret
    __ ret(0);
    __ p2align(16, 2);

    __ bind(L_zero);

// 	.p2align 4,, 2
// L(zero):
// 	xorl	%eax, %eax
//  376:	31 c0                	xor    eax,eax
    __ xorl(rax, rax);
// 	/* No ymm register was touched.  */
// 	ret
//  378:	c3                   	ret
//  379:	0f 1f 80 00 00 00 00 	nop    DWORD PTR [rax+0x0]
    __ ret(0);
    __ align(16);

    __ bind(L_between_8_15);

// 	.p2align 4
// L(between_8_15):
// 	movbe	(%rdi), %rax
//  380:	48 0f 38 f0 07       	movbe  rax,QWORD PTR [rdi]
    __ movzbl(rax, Address(rdi, 0));
// 	movbe	(%rsi), %rcx
//  385:	48 0f 38 f0 0e       	movbe  rcx,QWORD PTR [rsi]
    __ movzbl(rcx, Address(rsi, 0));
// 	subq	%rcx, %rax
//  38a:	48 29 c8             	sub    rax,rcx
    __ subq(rax, rcx);
// 	jnz	L(ret_nonzero)
//  38d:	75 e1                	jne    370 <__memcmp_avx2_movbe+0x370>
    __ jne_b(L_ret_nonzero);
// 	movbe	-8(%rdi, %rdx), %rax
//  38f:	48 0f 38 f0 44 17 f8 	movbe  rax,QWORD PTR [rdi+rdx*1-0x8]
    __ movzbl(rax, Address(rdi, rdx, Address::times_1, -0x8));
// 	movbe	-8(%rsi, %rdx), %rcx
//  396:	48 0f 38 f0 4c 16 f8 	movbe  rcx,QWORD PTR [rsi+rdx*1-0x8]
    __ movzbl(rcx, Address(rsi, rdx, Address::times_1, -0x8));
// 	subq	%rcx, %rax
//  39d:	48 29 c8             	sub    rax,rcx
    __ subq(rax, rcx);
// 	/* Fast path for return zero.  */
// 	jnz	L(ret_nonzero)
//  3a0:	75 ce                	jne    370 <__memcmp_avx2_movbe+0x370>
    __ jne_b(L_ret_nonzero);
// 	/* No ymm register was touched.  */
// 	ret
//  3a2:	c3                   	ret
// # endif
    __ ret(0);
    __ p2align(16, 10);

    __ bind(L_between_16_31);

// 	.p2align 4,, 10
// L(between_16_31):
// 	/* From 16 to 31 bytes.  No branch when size == 16.  */
// 	vmovdqu	(%rsi), %xmm2
//  3a3:	c5 fa 6f 16          	vmovdqu xmm2,XMMWORD PTR [rsi]
    __ movdqu(xmm2, Address(rsi, 0));
// 	VPCMPEQ	(%rdi), %xmm2, %xmm2
//  3a7:	c5 e9 74 17          	vpcmpeqb xmm2,xmm2,XMMWORD PTR [rdi]
    __ vpcmpeqb(xmm2, xmm2, Address(rdi, 0), Assembler::AVX_128bit);
// 	vpmovmskb %xmm2, %eax
//  3ab:	c5 f9 d7 c2          	vpmovmskb eax,xmm2
    __ vpmovmskb(rax, xmm2, Assembler::AVX_256bit);
// 	subl	$0xffff, %eax
//  3af:	2d ff ff 00 00       	sub    eax,0xffff
    __ subl(rax, 0xffff);
// 	jnz	L(return_vec_0)
//  3b4:	0f 85 26 fd ff ff    	jne    e0 <__memcmp_avx2_movbe+0xe0>
    __ jne(L_return_vec_0);

// 	/* Use overlapping loads to avoid branches.  */

// 	vmovdqu	-16(%rsi, %rdx), %xmm2
//  3ba:	c5 fa 6f 54 16 f0    	vmovdqu xmm2,XMMWORD PTR [rsi+rdx*1-0x10]
    __ movdqu(xmm2, Address(rsi, rdx, Address::times_1, -0x10));
// 	leaq	-16(%rdi, %rdx), %rdi
//  3c0:	48 8d 7c 17 f0       	lea    rdi,[rdi+rdx*1-0x10]
    __ leaq(rdi, Address(rdi, rdx, Address::times_1, -0x10));
// 	leaq	-16(%rsi, %rdx), %rsi
//  3c5:	48 8d 74 16 f0       	lea    rsi,[rsi+rdx*1-0x10]
    __ leaq(rsi, Address(rsi, rdx, Address::times_1, -0x10));
// 	VPCMPEQ	(%rdi), %xmm2, %xmm2
//  3ca:	c5 e9 74 17          	vpcmpeqb xmm2,xmm2,XMMWORD PTR [rdi]
    __ vpcmpeqb(xmm2, xmm2, Address(rdi, 0), Assembler::AVX_128bit);
// 	vpmovmskb %xmm2, %eax
//  3ce:	c5 f9 d7 c2          	vpmovmskb eax,xmm2
    __ vpmovmskb(rax, xmm2, Assembler::AVX_256bit);
// 	subl	$0xffff, %eax
//  3d2:	2d ff ff 00 00       	sub    eax,0xffff
    __ subl(rax, 0xffff);
// 	/* Fast path for return zero.  */
// 	jnz	L(return_vec_0)
//  3d7:	0f 85 03 fd ff ff    	jne    e0 <__memcmp_avx2_movbe+0xe0>
    __ jne(L_return_vec_0);
// 	/* No ymm register was touched.  */
// 	ret
//  3dd:	c3                   	ret
//  3de:	66 90                	xchg   ax,ax
// # else
    __ ret(0);
    __ align(16);

    __ bind(L_between_2_3);

// 	.p2align 4
// L(between_2_3):
// 	/* Load as big endian to avoid branches.  */
// 	movzwl	(%rdi), %eax
//  3e0:	0f b7 07             	movzx  eax,WORD PTR [rdi]
    __ movzwl(rax, Address(rdi, 0));
// 	movzwl	(%rsi), %ecx
//  3e3:	0f b7 0e             	movzx  ecx,WORD PTR [rsi]
    __ movzwl(rcx, Address(rsi, 0));
// 	bswap	%eax
//  3e6:	0f c8                	bswap  eax
    __ bswapl(rax);
// 	bswap	%ecx
//  3e8:	0f c9                	bswap  ecx
    __ bswapl(rcx);
// 	shrl	%eax
//  3ea:	d1 e8                	shr    eax,1
    __ shrl(rax, 1);
// 	shrl	%ecx
//  3ec:	d1 e9                	shr    ecx,1
    __ shrl(rcx, 1);
// 	movzbl	-1(%rdi, %rdx), %edi
//  3ee:	0f b6 7c 17 ff       	movzx  edi,BYTE PTR [rdi+rdx*1-0x1]
    __ movzbl(rdi, Address(rdi, rdx, Address::times_1, -0x1));
// 	movzbl	-1(%rsi, %rdx), %esi
//  3f3:	0f b6 74 16 ff       	movzx  esi,BYTE PTR [rsi+rdx*1-0x1]
    __ movzbl(rsi, Address(rsi, rdx, Address::times_1, -0x1));
// 	orl	%edi, %eax
//  3f8:	09 f8                	or     eax,edi
    __ orl(rax, rdi);
// 	orl	%esi, %ecx
//  3fa:	09 f1                	or     ecx,esi
    __ orl(rcx, rsi);
// 	/* Subtraction is okay because the upper bit is zero.  */
// 	subl	%ecx, %eax
//  3fc:	29 c8                	sub    eax,ecx
    __ subl(rax, rcx);
// 	/* No ymm register was touched.  */
// 	ret
//  3fe:	c3                   	ret
    __ ret(0);

// End of assembler dump.


    {
      Label L_return_vzeroupper, L_zero, L_first_vec_x1, L_first_vec_x2;
      Label L_first_vec_x3, L_first_vec_x4, L_aligned_more, L_cross_page_continue;
      Label L_loop_4x_vec, L_last_vec_x0, L_last_vec_x1, L_zero_end, L_cross_page_boundary;

      __ align(CodeEntryAlignment);
      __ bind(strchr_avx2);

// Disassembly of section .text.avx:

// 0000000000000000 <__strchr_avx2>:

// # define VEC_SIZE 32
// # define PAGE_SIZE 4096

// 	.section SECTION(.text),"ax",@progbits
// ENTRY_P2ALIGN (STRCHR, 5)
//    0:	f3 0f 1e fa          	endbr64
// 	/* Broadcast CHAR to YMM0.	*/
// 	vmovd	%esi, %xmm0
//    4:	c5 f9 6e c6          	vmovd  xmm0,esi
      __ movdl(xmm0, rsi);
// 	movl	%edi, %eax
//    8:	89 f8                	mov    eax,edi
      __ movl(rax, rdi);
// 	andl	$(PAGE_SIZE - 1), %eax
//    a:	25 ff 0f 00 00       	and    eax,0xfff
      __ andl(rax, 0xfff);
// 	VPBROADCAST	%xmm0, %ymm0
//    f:	c4 e2 7d 78 c0       	vpbroadcastb ymm0,xmm0
      __ vpbroadcastb(xmm0, xmm0, Assembler::AVX_256bit);
// 	vpxor	%xmm1, %xmm1, %xmm1
//   14:	c5 f1 ef c9          	vpxor  xmm1,xmm1,xmm1
      __ vpxor(xmm1, xmm1, xmm1, Assembler::AVX_128bit);

// 	/* Check if we cross page boundary with one vector load.  */
// 	cmpl	$(PAGE_SIZE - VEC_SIZE), %eax
//   18:	3d e0 0f 00 00       	cmp    eax,0xfe0
      __ cmpl(rax, 0xfe0);
// 	ja	L(cross_page_boundary)
//   1d:	0f 87 dd 01 00 00    	ja     200 <__strchr_avx2+0x200>
      __ ja(L_cross_page_boundary);

// 	/* Check the first VEC_SIZE bytes.	Search for both CHAR and the
// 	   null byte.  */
// 	vmovdqu	(%rdi), %ymm2
//   23:	c5 fe 6f 17          	vmovdqu ymm2,YMMWORD PTR [rdi]
      __ vmovdqu(xmm2, Address(rdi, 0));
// 	VPCMPEQ	%ymm2, %ymm0, %ymm3
//   27:	c5 fd 74 da          	vpcmpeqb ymm3,ymm0,ymm2
      __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//   2b:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpor	%ymm3, %ymm2, %ymm3
//   2f:	c5 ed eb db          	vpor   ymm3,ymm2,ymm3
      __ vpor(xmm3, xmm2, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//   33:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//   37:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jz	L(aligned_more)
//   39:	0f 84 81 00 00 00    	je     c0 <__strchr_avx2+0xc0>
      __ je(L_aligned_more);
// 	tzcntl	%eax, %eax
//   3f:	f3 0f bc c0          	tzcnt  eax,eax
      __ tzcntl(rax, rax);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.  */
// 	cmp	(%rdi, %rax), %CHAR_REG
//   43:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	   null. Since this branch will be 100% predictive of the user
// 	   branch a branch miss here should save what otherwise would
// 	   be branch miss in the user code. Otherwise using a branch 1)
// 	   saves code size and 2) is faster in highly predictable
// 	   environments.  */
// 	jne	L(zero)
//   47:	75 07                	jne    50 <__strchr_avx2+0x50>
      __ jne_b(L_zero);
// # endif
// 	addq	%rdi, %rax
//   49:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// L(return_vzeroupper):
      __ bind(L_return_vzeroupper);
// 	ZERO_UPPER_VEC_REGISTERS_RETURN
//   4c:	c5 f8 77             	vzeroupper
//   4f:	c3                   	ret
      __ vzeroupper();
      __ ret(0);

// # ifndef USE_AS_STRCHRNUL
// L(zero):
      __ bind(L_zero);
// 	xorl	%eax, %eax
//   50:	31 c0                	xor    eax,eax
      __ xorl(rax, rax);
// 	VZEROUPPER_RETURN
//   52:	c5 f8 77             	vzeroupper
//   55:	c3                   	ret
//   56:	66 2e 0f 1f 84 00 00 	cs nop WORD PTR [rax+rax*1+0x0]
//   5d:	00 00 00
      __ vzeroupper();
      __ ret(0);
      __ align(16);

// 	.p2align 4
// L(first_vec_x1):
      __ bind(L_first_vec_x1);
// 	/* Use bsf to save code size.  */
// 	bsfl	%eax, %eax
//   60:	0f bc c0             	bsf    eax,eax
      __ bsfl(rax, rax);
// 	incq	%rdi
//   63:	48 ff c7             	inc    rdi
      __ incrementq(rdi);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//   66:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero)
//   6a:	75 e4                	jne    50 <__strchr_avx2+0x50>
      __ jne_b(L_zero);
// # endif
// 	addq	%rdi, %rax
//   6c:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//   6f:	c5 f8 77             	vzeroupper
//   72:	c3                   	ret
      __ vzeroupper();
      __ ret(0);
      __ p2align(16, 10);

// 	.p2align 4,, 10
// L(first_vec_x2):
      __ bind(L_first_vec_x2);
// 	/* Use bsf to save code size.  */
// 	bsfl	%eax, %eax
//   73:	0f bc c0             	bsf    eax,eax
      __ bsfl(rax, rax);
// 	addq	$(VEC_SIZE + 1), %rdi
//   76:	48 83 c7 21          	add    rdi,0x21
      __ addq(rdi, 0x21);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//   7a:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero)
//   7e:	75 d0                	jne    50 <__strchr_avx2+0x50>
      __ jne_b(L_zero);
// # endif
// 	addq	%rdi, %rax
//   80:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//   83:	c5 f8 77             	vzeroupper
//   86:	c3                   	ret
      __ vzeroupper();
      __ ret(0);
      __ p2align(16, 8);

// 	.p2align 4,, 8
// L(first_vec_x3):
      __ bind(L_first_vec_x3);
// 	/* Use bsf to save code size.  */
// 	bsfl	%eax, %eax
//   87:	0f bc c0             	bsf    eax,eax
      __ bsfl(rax, rax);
// 	addq	$(VEC_SIZE * 2 + 1), %rdi
//   8a:	48 83 c7 41          	add    rdi,0x41
      __ addq(rdi, 0x41);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//   8e:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero)
//   92:	75 bc                	jne    50 <__strchr_avx2+0x50>
      __ jne_b(L_zero);
// # endif
// 	addq	%rdi, %rax
//   94:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//   97:	c5 f8 77             	vzeroupper
//   9a:	c3                   	ret
//   9b:	0f 1f 44 00 00       	nop    DWORD PTR [rax+rax*1+0x0]
      __ vzeroupper();
      __ ret(0);
      __ p2align(16, 10);

// 	.p2align 4,, 10
// L(first_vec_x4):
      __ bind(L_first_vec_x4);
// 	/* Use bsf to save code size.  */
// 	bsfl	%eax, %eax
//   a0:	0f bc c0             	bsf    eax,eax
      __ bsfl(rax, rax);
// 	addq	$(VEC_SIZE * 3 + 1), %rdi
//   a3:	48 83 c7 61          	add    rdi,0x61
      __ addq(rdi, 0x61);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//   a7:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero)
//   ab:	75 a3                	jne    50 <__strchr_avx2+0x50>
      __ jne_b(L_zero);
// # endif
// 	addq	%rdi, %rax
//   ad:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//   b0:	c5 f8 77             	vzeroupper
//   b3:	c3                   	ret
//   b4:	66 66 2e 0f 1f 84 00 	data16 cs nop WORD PTR [rax+rax*1+0x0]
//   bb:	00 00 00 00
//   bf:	90                   	nop
      __ vzeroupper();
      __ ret(0);
      __ align(16);

// 	.p2align 4
// L(aligned_more):
      __ bind(L_aligned_more);
// 	/* Align data to VEC_SIZE - 1. This is the same number of
// 	   instructions as using andq -VEC_SIZE but saves 4 bytes of code
// 	   on x4 check.  */
// 	orq	$(VEC_SIZE - 1), %rdi
//   c0:	48 83 cf 1f          	or     rdi,0x1f
      __ orq(rdi, 0x1f);
// L(cross_page_continue):
      __ bind(L_cross_page_continue);
// 	/* Check the next 4 * VEC_SIZE.  Only one VEC_SIZE at a time
// 	   since data is only aligned to VEC_SIZE.  */
// 	vmovdqa	1(%rdi), %ymm2
//   c4:	c5 fd 6f 57 01       	vmovdqa ymm2,YMMWORD PTR [rdi+0x1]
      __ vmovdqu(xmm2, Address(rdi, 0x1));
// 	VPCMPEQ	%ymm2, %ymm0, %ymm3
//   c9:	c5 fd 74 da          	vpcmpeqb ymm3,ymm0,ymm2
      __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//   cd:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpor	%ymm3, %ymm2, %ymm3
//   d1:	c5 ed eb db          	vpor   ymm3,ymm2,ymm3
      __ vpor(xmm3, xmm2, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//   d5:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//   d9:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jnz	L(first_vec_x1)
//   db:	75 83                	jne    60 <__strchr_avx2+0x60>
      __ jne_b(L_first_vec_x1);

// 	vmovdqa	(VEC_SIZE + 1)(%rdi), %ymm2
//   dd:	c5 fd 6f 57 21       	vmovdqa ymm2,YMMWORD PTR [rdi+0x21]
      __ vmovdqu(xmm2, Address(rdi, 0x21));
// 	VPCMPEQ	%ymm2, %ymm0, %ymm3
//   e2:	c5 fd 74 da          	vpcmpeqb ymm3,ymm0,ymm2
      __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//   e6:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpor	%ymm3, %ymm2, %ymm3
//   ea:	c5 ed eb db          	vpor   ymm3,ymm2,ymm3
      __ vpor(xmm3, xmm2, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//   ee:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//   f2:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jnz	L(first_vec_x2)
//   f4:	0f 85 79 ff ff ff    	jne    73 <__strchr_avx2+0x73>
      __ jne(L_first_vec_x2);

// 	vmovdqa	(VEC_SIZE * 2 + 1)(%rdi), %ymm2
//   fa:	c5 fd 6f 57 41       	vmovdqa ymm2,YMMWORD PTR [rdi+0x41]
      __ vmovdqu(xmm2, Address(rdi, 0x41));
// 	VPCMPEQ	%ymm2, %ymm0, %ymm3
//   ff:	c5 fd 74 da          	vpcmpeqb ymm3,ymm0,ymm2
      __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//  103:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpor	%ymm3, %ymm2, %ymm3
//  107:	c5 ed eb db          	vpor   ymm3,ymm2,ymm3
      __ vpor(xmm3, xmm2, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//  10b:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//  10f:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jnz	L(first_vec_x3)
//  111:	0f 85 70 ff ff ff    	jne    87 <__strchr_avx2+0x87>
      __ jne(L_first_vec_x3);

// 	vmovdqa	(VEC_SIZE * 3 + 1)(%rdi), %ymm2
//  117:	c5 fd 6f 57 61       	vmovdqa ymm2,YMMWORD PTR [rdi+0x61]
      __ vmovdqu(xmm2, Address(rdi, 0x61));
// 	VPCMPEQ	%ymm2, %ymm0, %ymm3
//  11c:	c5 fd 74 da          	vpcmpeqb ymm3,ymm0,ymm2
      __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//  120:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpor	%ymm3, %ymm2, %ymm3
//  124:	c5 ed eb db          	vpor   ymm3,ymm2,ymm3
      __ vpor(xmm3, xmm2, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//  128:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//  12c:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jnz	L(first_vec_x4)
//  12e:	0f 85 6c ff ff ff    	jne    a0 <__strchr_avx2+0xa0>
      __ jne(L_first_vec_x4);
// 	/* Align data to VEC_SIZE * 4 - 1.  */
// 	incq	%rdi
//  134:	48 ff c7             	inc    rdi
      __ incrementq(rdi);
// 	orq	$(VEC_SIZE * 4 - 1), %rdi
//  137:	48 83 cf 7f          	or     rdi,0x7f
//  13b:	0f 1f 44 00 00       	nop    DWORD PTR [rax+rax*1+0x0]
      __ orq(rdi, 0x7f);
// 	.p2align 4
// L(loop_4x_vec):
      __ bind(L_loop_4x_vec);
// 	/* Compare 4 * VEC at a time forward.  */
// 	vmovdqa	1(%rdi), %ymm6
//  140:	c5 fd 6f 77 01       	vmovdqa ymm6,YMMWORD PTR [rdi+0x1]
      __ vmovdqu(xmm6, Address(rdi, 0x1));
// 	vmovdqa	(VEC_SIZE + 1)(%rdi), %ymm7
//  145:	c5 fd 6f 7f 21       	vmovdqa ymm7,YMMWORD PTR [rdi+0x21]
      __ vmovdqu(xmm7, Address(rdi, 0x21));

// 	/* Leaves only CHARS matching esi as 0.	 */
// 	vpxor	%ymm6, %ymm0, %ymm2
//  14a:	c5 fd ef d6          	vpxor  ymm2,ymm0,ymm6
      __ vpxor(xmm2, xmm0, xmm6, Assembler::AVX_256bit);
// 	vpxor	%ymm7, %ymm0, %ymm3
//  14e:	c5 fd ef df          	vpxor  ymm3,ymm0,ymm7
      __ vpxor(xmm3, xmm0, xmm7, Assembler::AVX_256bit);

// 	VPMINU	%ymm2, %ymm6, %ymm2
//  152:	c5 cd da d2          	vpminub ymm2,ymm6,ymm2
      __ vpminub(xmm2, xmm6, xmm2, Assembler::AVX_256bit);
// 	VPMINU	%ymm3, %ymm7, %ymm3
//  156:	c5 c5 da db          	vpminub ymm3,ymm7,ymm3
      __ vpminub(xmm3, xmm7, xmm3, Assembler::AVX_256bit);

// 	vmovdqa	(VEC_SIZE * 2 + 1)(%rdi), %ymm6
//  15a:	c5 fd 6f 77 41       	vmovdqa ymm6,YMMWORD PTR [rdi+0x41]
      __ vmovdqu(xmm6, Address(rdi, 0x41));
// 	vmovdqa	(VEC_SIZE * 3 + 1)(%rdi), %ymm7
//  15f:	c5 fd 6f 7f 61       	vmovdqa ymm7,YMMWORD PTR [rdi+0x61]
      __ vmovdqu(xmm7, Address(rdi, 0x61));

// 	vpxor	%ymm6, %ymm0, %ymm4
//  164:	c5 fd ef e6          	vpxor  ymm4,ymm0,ymm6
      __ vpxor(xmm4, xmm0, xmm6, Assembler::AVX_256bit);
// 	vpxor	%ymm7, %ymm0, %ymm5
//  168:	c5 fd ef ef          	vpxor  ymm5,ymm0,ymm7
      __ vpxor(xmm5, xmm0, xmm7, Assembler::AVX_256bit);

// 	VPMINU	%ymm4, %ymm6, %ymm4
//  16c:	c5 cd da e4          	vpminub ymm4,ymm6,ymm4
      __ vpminub(xmm4, xmm6, xmm4, Assembler::AVX_256bit);
// 	VPMINU	%ymm5, %ymm7, %ymm5
//  170:	c5 c5 da ed          	vpminub ymm5,ymm7,ymm5
      __ vpminub(xmm5, xmm7, xmm5, Assembler::AVX_256bit);

// 	VPMINU	%ymm2, %ymm3, %ymm6
//  174:	c5 e5 da f2          	vpminub ymm6,ymm3,ymm2
      __ vpminub(xmm6, xmm3, xmm2, Assembler::AVX_256bit);
// 	VPMINU	%ymm4, %ymm5, %ymm7
//  178:	c5 d5 da fc          	vpminub ymm7,ymm5,ymm4
      __ vpminub(xmm7, xmm5, xmm4, Assembler::AVX_256bit);

// 	VPMINU	%ymm6, %ymm7, %ymm7
//  17c:	c5 c5 da fe          	vpminub ymm7,ymm7,ymm6
      __ vpminub(xmm7, xmm7, xmm6, Assembler::AVX_256bit);

// 	VPCMPEQ	%ymm7, %ymm1, %ymm7
//  180:	c5 f5 74 ff          	vpcmpeqb ymm7,ymm1,ymm7
      __ vpcmpeqb(xmm7, xmm1, xmm7, Assembler::AVX_256bit);
// 	vpmovmskb %ymm7, %ecx
//  184:	c5 fd d7 cf          	vpmovmskb ecx,ymm7
      __ vpmovmskb(rcx, xmm7, Assembler::AVX_256bit);
// 	subq	$-(VEC_SIZE * 4), %rdi
//  188:	48 83 ef 80          	sub    rdi,0xffffffffffffff80
      __ subq(rdi, -128);
// 	testl	%ecx, %ecx
//  18c:	85 c9                	test   ecx,ecx
      __ testl(rcx, rcx);
// 	jz	L(loop_4x_vec)
//  18e:	74 b0                	je     140 <__strchr_avx2+0x140>
      __ je_b(L_loop_4x_vec);

// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//  190:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpmovmskb %ymm2, %eax
//  194:	c5 fd d7 c2          	vpmovmskb eax,ymm2
      __ vpmovmskb(rax, xmm2, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//  198:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jnz	L(last_vec_x0)
//  19a:	75 34                	jne    1d0 <__strchr_avx2+0x1d0>
      __ jne_b(L_last_vec_x0);


// 	VPCMPEQ	%ymm3, %ymm1, %ymm3
//  19c:	c5 f5 74 db          	vpcmpeqb ymm3,ymm1,ymm3
      __ vpcmpeqb(xmm3, xmm1, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//  1a0:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	testl	%eax, %eax
//  1a4:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jnz	L(last_vec_x1)
//  1a6:	75 3c                	jne    1e4 <__strchr_avx2+0x1e4>
      __ jne_b(L_last_vec_x1);

// 	VPCMPEQ	%ymm4, %ymm1, %ymm4
//  1a8:	c5 f5 74 e4          	vpcmpeqb ymm4,ymm1,ymm4
      __ vpcmpeqb(xmm4, xmm1, xmm4, Assembler::AVX_256bit);
// 	vpmovmskb %ymm4, %eax
//  1ac:	c5 fd d7 c4          	vpmovmskb eax,ymm4
      __ vpmovmskb(rax, xmm4, Assembler::AVX_256bit);
// 	/* rcx has combined result from all 4 VEC. It will only be used
// 	   if the first 3 other VEC all did not contain a match.  */
// 	salq	$32, %rcx
//  1b0:	48 c1 e1 20          	shl    rcx,0x20
      __ shlq(rcx, 0x20);
// 	orq	%rcx, %rax
//  1b4:	48 09 c8             	or     rax,rcx
      __ orq(rax,rcx);
// 	tzcntq	%rax, %rax
//  1b7:	f3 48 0f bc c0       	tzcnt  rax,rax
      __ tzcntq(rax, rax);
// 	subq	$(VEC_SIZE * 2 - 1), %rdi
//  1bc:	48 83 ef 3f          	sub    rdi,0x3f
      __ subq(rdi, 0x3f);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//  1c0:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero_end)
//  1c4:	75 33                	jne    1f9 <__strchr_avx2+0x1f9>
      __ jne_b(L_zero_end);
// # endif
// 	addq	%rdi, %rax
//  1c6:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//  1c9:	c5 f8 77             	vzeroupper
//  1cc:	c3                   	ret
//  1cd:	0f 1f 00             	nop    DWORD PTR [rax]
      __ vzeroupper();
      __ ret(0);
      __ p2align(16, 10);


// 	.p2align 4,, 10
// L(last_vec_x0):
      __ bind(L_last_vec_x0);
// 	/* Use bsf to save code size.  */
// 	bsfl	%eax, %eax
//  1d0:	0f bc c0             	bsf    eax,eax
      __ bsfl(rax, rax);
// 	addq	$-(VEC_SIZE * 4 - 1), %rdi
//  1d3:	48 83 c7 81          	add    rdi,0xffffffffffffff81
      __ addq(rdi, -127);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//  1d7:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero_end)
//  1db:	75 1c                	jne    1f9 <__strchr_avx2+0x1f9>
      __ jne_b(L_zero_end);
// # endif
// 	addq	%rdi, %rax
//  1dd:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//  1e0:	c5 f8 77             	vzeroupper
//  1e3:	c3                   	ret
      __ vzeroupper();
      __ ret(0);
      __ p2align(16, 10);


// 	.p2align 4,, 10
// L(last_vec_x1):
      __ bind(L_last_vec_x1);
// 	tzcntl	%eax, %eax
//  1e4:	f3 0f bc c0          	tzcnt  eax,eax
      __ tzcntl(rax, rax);
// 	subq	$(VEC_SIZE * 3 - 1), %rdi
//  1e8:	48 83 ef 5f          	sub    rdi,0x5f
      __ subq(rdi, 0x5f);
// # ifndef USE_AS_STRCHRNUL
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdi, %rax), %CHAR_REG
//  1ec:	40 3a 34 07          	cmp    sil,BYTE PTR [rdi+rax*1]
      __ cmpb(rsi, Address(rdi, rax, Address::times_1));
// 	jne	L(zero_end)
//  1f0:	75 07                	jne    1f9 <__strchr_avx2+0x1f9>
      __ jne_b(L_zero_end);
// # endif
// 	addq	%rdi, %rax
//  1f2:	48 01 f8             	add    rax,rdi
      __ addq(rax, rdi);
// 	VZEROUPPER_RETURN
//  1f5:	c5 f8 77             	vzeroupper
//  1f8:	c3                   	ret
      __ vzeroupper();
      __ ret(0);

// # ifndef USE_AS_STRCHRNUL
// L(zero_end):
      __ bind(L_zero_end);
// 	xorl	%eax, %eax
//  1f9:	31 c0                	xor    eax,eax
      __ xorq(rax, rax);
// 	VZEROUPPER_RETURN
//  1fb:	c5 f8 77             	vzeroupper
//  1fe:	c3                   	ret
//  1ff:	90                   	nop
// # endif
      __ vzeroupper();
      __ ret(0);
      __ p2align(16, 8);

// 	/* Cold case for crossing page with first load.	 */
// 	.p2align 4,, 8
// L(cross_page_boundary):
      __ bind(L_cross_page_boundary);
// 	movq	%rdi, %rdx
//  200:	48 89 fa             	mov    rdx,rdi
      __ movq(rdx, rdi);
// 	/* Align rdi to VEC_SIZE - 1.  */
// 	orq	$(VEC_SIZE - 1), %rdi
//  203:	48 83 cf 1f          	or     rdi,0x1f
      __ orq(rdi, 0x1f);
// 	vmovdqa	-(VEC_SIZE - 1)(%rdi), %ymm2
//  207:	c5 fd 6f 57 e1       	vmovdqa ymm2,YMMWORD PTR [rdi-0x1f]
      __ vmovdqu(xmm2, Address(rdi, -0x1f));
// 	VPCMPEQ	%ymm2, %ymm0, %ymm3
//  20c:	c5 fd 74 da          	vpcmpeqb ymm3,ymm0,ymm2
      __ vpcmpeqb(xmm3, xmm0, xmm2, Assembler::AVX_256bit);
// 	VPCMPEQ	%ymm2, %ymm1, %ymm2
//  210:	c5 f5 74 d2          	vpcmpeqb ymm2,ymm1,ymm2
      __ vpcmpeqb(xmm2, xmm1, xmm2, Assembler::AVX_256bit);
// 	vpor	%ymm3, %ymm2, %ymm3
//  214:	c5 ed eb db          	vpor   ymm3,ymm2,ymm3
      __ vpor(xmm3, xmm2, xmm3, Assembler::AVX_256bit);
// 	vpmovmskb %ymm3, %eax
//  218:	c5 fd d7 c3          	vpmovmskb eax,ymm3
      __ vpmovmskb(rax, xmm3, Assembler::AVX_256bit);
// 	/* Remove the leading bytes. sarxl only uses bits [5:0] of COUNT
// 	   so no need to manually mod edx.  */
// 	sarxl	%edx, %eax, %eax
//  21c:	c4 e2 6a f7 c0       	sarx   eax,eax,edx
      __ sarxl(rax, rax, rdx);
// 	testl	%eax, %eax
//  221:	85 c0                	test   eax,eax
      __ testl(rax, rax);
// 	jz	L(cross_page_continue)
//  223:	0f 84 9b fe ff ff    	je     c4 <__strchr_avx2+0xc4>
      __ je(L_cross_page_continue);
// 	tzcntl	%eax, %eax
//  229:	f3 0f bc c0          	tzcnt  eax,eax
      __ tzcntl(rax, rax);
// # ifndef USE_AS_STRCHRNUL
// 	xorl	%ecx, %ecx
//  22d:	31 c9                	xor    ecx,ecx
      __ xorl(rcx, rcx);
// 	/* Found CHAR or the null byte.	 */
// 	cmp	(%rdx, %rax), %CHAR_REG
//  22f:	40 3a 34 02          	cmp    sil,BYTE PTR [rdx+rax*1]
      __ cmpb(rsi, Address(rdx, rax, Address::times_1));
// 	jne	L(zero_end)
//  233:	75 c4                	jne    1f9 <__strchr_avx2+0x1f9>
      __ jne_b(L_zero_end);
// # endif
// 	addq	%rdx, %rax
//  235:	48 01 d0             	add    rax,rdx
      __ addq(rax, rdx);
// 	VZEROUPPER_RETURN
//  238:	c5 f8 77             	vzeroupper
//  23b:	c3                   	ret
      __ vzeroupper();
      __ ret(0);
    }



  return start;
}

#undef __
#endif