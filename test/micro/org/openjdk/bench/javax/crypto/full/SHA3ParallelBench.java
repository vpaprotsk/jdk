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
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Random;

@Fork(value = 1, jvmArgs = {"--add-opens", "java.base/sun.security.provider=ALL-UNNAMED"})
@State(Scope.Thread)
@BenchmarkMode(Mode.Throughput)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
public class SHA3ParallelBench {
    long[] state1, state2, state3, state4;
    private MethodHandle doubleKeccakH, quadKeccakH;

    @Setup
    public void setup() throws Exception {
        MethodHandles.Lookup lookup = MethodHandles.lookup();
        Class<?> kClazz = Class.forName("sun.security.provider.SHA3Parallel");
        Method m = kClazz.getDeclaredMethod("doubleKeccak", long[].class, long[].class);
        m.setAccessible(true);
        doubleKeccakH = lookup.unreflect(m);
        m = kClazz.getDeclaredMethod("quadKeccak", long[].class, long[].class, long[].class, long[].class);
        m.setAccessible(true);
        quadKeccakH = lookup.unreflect(m);

        Random rnd = new Random();
        state1 = new long[25];
        state2 = new long[25];
        state3 = new long[25];
        state4 = new long[25];

        for (int i = 0; i<state1.length; i++) {
            state1[i] = rnd.nextLong();
            state2[i] = rnd.nextLong();
            state3[i] = rnd.nextLong();
            state4[i] = rnd.nextLong();
        }
    }

    @Benchmark
    public void doubleKeccak() throws Throwable {
        doubleKeccakH.invoke(state1, state2);
    }

    @Benchmark
    public void quadKeccak() throws Throwable {
        quadKeccakH.invoke(state1, state2, state4, state4);
    }
}
