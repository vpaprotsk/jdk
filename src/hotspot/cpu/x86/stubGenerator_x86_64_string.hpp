/*
 * Copyright (c) 2024, Intel Corporation. All rights reserved.
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

class StringStubAVX {
  private:
    const size_t fixed_needle_sizes = 10;
    address fixed_table[fixed_needle_sizes]; 
    address generic_entry;
    Address::scale scale;

    StrIntrinsicNode::ArgEncoding _encoding;
    MacroAssembler *_masm;
    StrAVXIntrinsicStub(StrIntrinsicNode::ArgEncoding encoding, MacroAssembler *masm):_encoding(encoding), _masm(masm){}

    void generate_stub();
    void generate_fixed();

    void compare();
    void broadcast();
    //...
    
  public:
    address generic_stub_address() { return generic_entry;}
    // address fixed_case_address(int needle_size) { return small_table[needle_size];}
    // address fixed_case_address(int needle_size) { return large_table[needle_size];}
}