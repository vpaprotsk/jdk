/*
 * Copyright (c) 2015, 2025, Oracle and/or its affiliates. All rights reserved.
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
 */
package org.openjdk.bench.javax.crypto.full;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.Fork;

// import java.lang.invoke.MethodHandle;

// import javax.crypto.BadPaddingException;
// import javax.crypto.Cipher;
// import javax.crypto.IllegalBlockSizeException;
// import javax.crypto.NoSuchPaddingException;
// import javax.crypto.spec.SecretKeySpec;
// import java.security.InvalidAlgorithmParameterException;
// import java.security.InvalidKeyException;
// import java.security.NoSuchAlgorithmException;
// import java.security.spec.InvalidParameterSpecException;

import java.util.Random;

@Fork(value = 1, jvmArgs = {"--add-opens", "java.base/sun.security.provider=ALL-UNNAMED"})
public class SHA3Bench extends CryptoBase {
    long[] state1, state2, state3, state4, state5, state6, state7, state8;

    @Setup
    public void setup() {
        Random rnd = new Random();
        state1 = new long[25];
        state2 = new long[25];
        state3 = new long[25];
        state4 = new long[25];
        state5 = new long[25];
        state6 = new long[25];
        state7 = new long[25];
        state8 = new long[25];

        for (int i = 0; i<state1.length; i++) {
            state1[i] = rnd.nextLong();
            state2[i] = rnd.nextLong();
            state3[i] = rnd.nextLong();
            state4[i] = rnd.nextLong();
            state5[i] = rnd.nextLong();
            state6[i] = rnd.nextLong();
            state7[i] = rnd.nextLong();
            state8[i] = rnd.nextLong();
        }
    }

    @Benchmark
    public void doubleKeccak() {
        sun.security.provider.SHA3Parallel.doubleKeccak(state1, state2);
    }

    @Benchmark
    public void quadKeccak() {
        sun.security.provider.SHA3Parallel.quadKeccak(state1, state2, state4, state4);
    }

    @Benchmark
    public void eightKeccak() {
        sun.security.provider.SHA3Parallel.eightKeccak(state1, state2, state4, state4, state5, state6, state7, state8);
    }
}
