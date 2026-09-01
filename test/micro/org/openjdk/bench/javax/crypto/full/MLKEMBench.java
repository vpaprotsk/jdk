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
package org.openjdk.bench.javax.crypto.full;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import java.util.concurrent.TimeUnit;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Random;

@Fork(value = 1, jvmArgs = {"--add-opens", "java.base/com.sun.crypto.provider=ALL-UNNAMED"})
@State(Scope.Thread)
@BenchmarkMode(Mode.Throughput)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
public class MLKEMBench {
    private static final int ML_KEM_Q = 3329;
    private MethodHandle implKyberNttMult;
    private MethodHandle implKyberNtt;
    private MethodHandle inverseNtt;
    private MethodHandle implKyber12To16;

    private short[] result, ntta, nttb, parsed;
    private byte[] condensed;
    private short[] montZetasForVectorNttArr;
    private short[] montZetasForVectorNttMultArr;
    private short[] montZetasForVectorInverseNttArr;

    @Setup
    public void setup() throws Exception {
        MethodHandles.Lookup lookup = MethodHandles.lookup();

        Class<?> kClazz = Class.forName("com.sun.crypto.provider.ML_KEM");
        Method m = kClazz.getDeclaredMethod("implKyberNttMult",
           short[].class, short[].class, short[].class, short[].class);
        m.setAccessible(true);
        implKyberNttMult = lookup.unreflect(m);

        m = kClazz.getDeclaredMethod("implKyberNtt",
           short[].class, short[].class);
        m.setAccessible(true);
        implKyberNtt = lookup.unreflect(m);

        m = kClazz.getDeclaredMethod("implKyberInverseNtt", short[].class, short[].class);
        m.setAccessible(true);
        inverseNtt = lookup.unreflect(m);

        m = kClazz.getDeclaredMethod("implKyber12To16",
           byte[].class, int.class, short[].class, int.class);
        m.setAccessible(true);
        implKyber12To16 = lookup.unreflect(m);

        Field f = kClazz.getDeclaredField("montZetasForVectorNttArr");
        f.setAccessible(true);
        montZetasForVectorNttArr = (short[]) f.get(null);

        f = kClazz.getDeclaredField("montZetasForVectorNttMultArr");
        f.setAccessible(true);
        montZetasForVectorNttMultArr = (short[]) f.get(null);

        f = kClazz.getDeclaredField("montZetasForVectorInverseNttArr");
        f.setAccessible(true);
        montZetasForVectorInverseNttArr = (short[]) f.get(null);

        Random rnd = new Random();
        result = new short[256];
        ntta = new short[256];
        nttb = new short[256];
        for (int i = 0; i<256; i++) {
            ntta[i] = (short) (rnd.nextInt(2 * ML_KEM_Q) - ML_KEM_Q);
            nttb[i] = (short) (rnd.nextInt(2 * ML_KEM_Q) - ML_KEM_Q);
        }
        parsed = new short[256];
        condensed = new byte[3 * 256 / 2];
        rnd.nextBytes(condensed);
    }

    @Benchmark
    public void kyberNttMult() throws Throwable {
        implKyberNttMult.invoke(result, ntta, nttb, montZetasForVectorNttMultArr);
    }

    @Benchmark
    public void kyberNtt() throws Throwable {
        implKyberNtt.invoke(ntta, montZetasForVectorNttArr);
    }

    @Benchmark
    public void kyberInverseNtt() throws Throwable {
        inverseNtt.invoke(ntta, montZetasForVectorInverseNttArr);
    }

    @Benchmark
    public void kyber12To16() throws Throwable {
        implKyber12To16.invoke(condensed, 0, parsed, 256);
    }
}