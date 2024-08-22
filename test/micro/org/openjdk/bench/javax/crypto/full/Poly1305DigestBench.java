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
 */
package org.openjdk.bench.javax.crypto.full;

import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.infra.Blackhole;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.security.Key;
import java.security.spec.AlgorithmParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Warmup;
import org.openjdk.jmh.annotations.Measurement;
import java.nio.ByteBuffer;

@Measurement(iterations = 3, time = 10)
@Warmup(iterations = 3, time = 10)
@Fork(value = 1, jvmArgsAppend = {"--add-opens", "java.base/com.sun.crypto.provider=ALL-UNNAMED"})
public class Poly1305DigestBench extends CryptoBase {
    public static final int SET_SIZE = 128;

    @Param({"64", "256", "1024", "" + 16*1024, "" + 1024*1024})
    int dataSize;

    public enum DataMethod{BYTE, DIRECT, HEAP}
    @Param({"BYTE", "DIRECT", "HEAP"})
    private DataMethod dataMethod;

    private byte[][] data;
    private ByteBuffer[] dataBuffer = new ByteBuffer[SET_SIZE];
    int index = 0;
    private static MethodHandle polyEngineInit, polyEngineUpdate, polyEngineUpdateBuf, polyEngineFinal;
    private static Object polyObj;

    static {
        try {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            Class<?> polyClazz = Class.forName("com.sun.crypto.provider.Poly1305");
            Constructor<?> constructor = polyClazz.getDeclaredConstructor();
            constructor.setAccessible(true);
            polyObj = constructor.newInstance();

            Method m = polyClazz.getDeclaredMethod("engineInit", Key.class, AlgorithmParameterSpec.class);
            m.setAccessible(true);
            polyEngineInit = lookup.unreflect(m);

            m = polyClazz.getDeclaredMethod("engineUpdate", byte[].class, int.class, int.class);
            m.setAccessible(true);
            polyEngineUpdate = lookup.unreflect(m);

            m = polyClazz.getDeclaredMethod("engineUpdate", ByteBuffer.class);
            m.setAccessible(true);
            polyEngineUpdateBuf = lookup.unreflect(m);

            m = polyClazz.getDeclaredMethod("engineDoFinal");
            m.setAccessible(true);
            polyEngineFinal = lookup.unreflect(m);
        } catch (Throwable ex) {
            throw new RuntimeException(ex);
        }
    }

    @Setup
    public void setup() throws Throwable {
        setupProvider();
        data = fillRandom(new byte[SET_SIZE][dataSize]);
        polyEngineInit.invoke(polyObj, new SecretKeySpec(data[0], 0, 32, "Poly1305"), null);

        for (int i = 0; i < data.length; i++) {
            if (dataMethod == DataMethod.HEAP) {
                dataBuffer[i] = ByteBuffer.wrap(data[i]);
            } else if (dataMethod == DataMethod.DIRECT) {
                dataBuffer[i] = ByteBuffer.allocateDirect(data[i].length);
                dataBuffer[i].put(data[i]);
            }
        }
    }

    @Benchmark
    public void digest(Blackhole bh) throws Throwable {
        if (dataMethod == DataMethod.BYTE) {
            polyEngineUpdate.invoke(polyObj, data[index], 0, data[index].length);
        } else {
            polyEngineUpdateBuf.invoke(polyObj, dataBuffer[index]);
            dataBuffer[index].rewind();
        }
        bh.consume(polyEngineFinal.invoke(polyObj));
        index = (index +1) % SET_SIZE;
    }
}
