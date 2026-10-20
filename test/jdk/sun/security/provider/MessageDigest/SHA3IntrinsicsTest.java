/*
 * Copyright (c) 2025, Intel Corporation. All rights reserved.
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

import java.util.Arrays;
import java.util.Random;
import java.nio.ByteOrder;
import java.lang.invoke.VarHandle;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.util.HexFormat;

import sun.security.provider.SHA3;
import sun.security.provider.SHA3Parallel;

/*
 * @test
 * @library /test/lib
 * @key randomness
 * @modules java.base/sun.security.provider:+open
 * @run main/othervm/timeout=28800 SHA3IntrinsicsTest
 */

 /*
 * @test
 * @library /test/lib
 * @key randomness
 * @modules java.base/sun.security.provider:+open
 * @run main/othervm/timeout=28800 -XX:UseAVX=2 SHA3IntrinsicsTest
 */
public class SHA3IntrinsicsTest {
    private static MethodHandle doubleKeccakH, quadKeccakH, implCompressH, implMultiCompressH;
    private static Field bSize, state;

    public static void main(String[] args) throws Throwable {
        MethodHandles.Lookup lookup = MethodHandles.lookup();
        Class<?> kClazz = sun.security.provider.SHA3Parallel.class;
        Method m = kClazz.getDeclaredMethod("doubleKeccak", long[].class, long[].class);
        m.setAccessible(true);
        doubleKeccakH = lookup.unreflect(m);
        m = kClazz.getDeclaredMethod("quadKeccak", long[].class, long[].class, long[].class, long[].class);
        m.setAccessible(true);
        quadKeccakH = lookup.unreflect(m);

        kClazz = SHA3.class;
        m = kClazz.getDeclaredMethod("implCompress0", byte[].class, int.class);
        m.setAccessible(true);
        implCompressH = lookup.unreflect(m);
        state = kClazz.getDeclaredField("state");
        state.setAccessible(true);

        kClazz = kClazz.getSuperclass();
        bSize = kClazz.getDeclaredField("blockSize");
        bSize.setAccessible(true);
        
        m = kClazz.getDeclaredMethod("implCompressMultiBlock0", byte[].class, int.class, int.class);
        m.setAccessible(true);
        implMultiCompressH = lookup.unreflect(m);

        fuzzTest();
    }

    static int[] blockSizes = {72, 104, 136, 144, 168};
    
    static void fuzzTest() throws Throwable {
        SHA3.SHAKE256 sha3Instance = new SHA3.SHAKE256();
        Random rnd = new Random();
        long seed = rnd.nextLong();
        rnd.setSeed(seed);

        long[] state1 = new long[25];
        long[] state2 = new long[25];
        long[] state3 = new long[25];
        long[] state4 = new long[25];

        for (int i = 0; i<state1.length; i++) {
            state1[i] = rnd.nextLong();
            state2[i] = rnd.nextLong();
            state3[i] = rnd.nextLong();
            state4[i] = rnd.nextLong();
        }

        long[] state1Ref = state1.clone();
        long[] state2Ref = state2.clone();
        long[] state3Ref = state3.clone();
        long[] state4Ref = state4.clone();

        long[] state1copy1 = state1.clone();
        long[] state2copy1 = state2.clone();

        long[] state1copy2 = state1.clone();
        long[] state2copy2 = state2.clone();
        long[] state3copy2 = state3.clone();
        long[] state4copy2 = state4.clone();

        for (int i = 0; i<10000000; i++) {
            int len = blockSizes[rnd.nextInt(blockSizes.length)] * oneMany(rnd);

            for (int blockSize : blockSizes) {
                if (len<blockSize) continue;
                byte[] buf = new byte[len];
                rnd.nextBytes(buf);
                bSize.setInt(sha3Instance, blockSize);

                state.set(sha3Instance, state1);
                implCompressH.invoke(sha3Instance, buf, 0);
                implCompressRef(state1, blockSize, buf, 0);

                if (!java.util.Arrays.equals(state1, (long[])state.get(sha3Instance))) {
                    throw new RuntimeException("Fail " + i + "[blocksize "+blockSize+"] [length "+len+"]");
                }

                state.set(sha3Instance, state2);
                int r1 = (int)implMultiCompressH.invoke(sha3Instance, buf, 0, buf.length-blockSize);
                int r2 = implCompressMultiBlockRef(state2, blockSize, buf, 0, buf.length-blockSize);

                if (r1 != r2 || !java.util.Arrays.equals(state2, (long[])state.get(sha3Instance))) {
                    throw new RuntimeException("Fail testMultiCompress " + i + "[blocksize "+blockSize+"] [length "+len+"]");
                }
            }
            
            SHA3.keccak(state1Ref);
            SHA3.keccak(state2Ref);
            SHA3.keccak(state3Ref);
            SHA3.keccak(state4Ref);

            doubleKeccakH.invoke(state1copy1, state2copy1);

            if (!java.util.Arrays.equals(state1copy1, state1Ref)) {
                throw new RuntimeException("Fail doubleKeccak low " + i);
            } else if (!java.util.Arrays.equals(state2copy1, state2Ref)) {
                throw new RuntimeException("Fail doubleKeccak high " + i);
            }

            quadKeccakH.invoke(state1copy2, state2copy2, state3copy2, state4copy2);

            if (!java.util.Arrays.equals(state1copy2, state1Ref)) {
                throw new RuntimeException("Fail quadKeccak 1 " + i);
            } else 
            if (!java.util.Arrays.equals(state2copy2, state2Ref)) {
                throw new RuntimeException("Fail quadKeccak 2 " + i);
            } else if (!java.util.Arrays.equals(state3copy2, state3Ref)) {
                throw new RuntimeException("Fail quadKeccak 3 " + i);
            }
            if (!java.util.Arrays.equals(state4copy2, state4Ref)) {
                throw new RuntimeException("Fail quadKeccak 4 " + i);
            }
        }
        System.out.println("Passed");
    }

    static int oneMany(Random rnd) {
        switch(rnd.nextInt(2)) {
            case 0: return 1; // 50% chance returns 1
            default: return 1+rnd.nextInt(100);
        }
    }

    // Copied out from SHA3.java and DigestBase.java
    static final VarHandle asLittleEndian
            = MethodHandles.byteArrayViewVarHandle(long[].class,
            ByteOrder.LITTLE_ENDIAN).withInvokeExactBehavior();
            
    static int implCompressMultiBlockRef(long[] state, int blockSize, byte[] b, int ofs, int limit) {
        for (; ofs <= limit; ofs += blockSize) {
            implCompressRef(state, blockSize, b, ofs);
        }
        return ofs;
    }

    static void implCompressRef(long[] state, int blockSize, byte[] b, int ofs) {
        for (int i = 0; i < blockSize / 8; i++) {
            state[i] ^= (long) asLittleEndian.get(b, ofs);
            ofs += 8;
        }

        SHA3.keccak(state);
    }
}
