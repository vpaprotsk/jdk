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
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.Scope;
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
@State(Scope.Thread)
public class SHA3BufferBench {
    @Param({"72", "104", "136", "144", "168"})
    private int blockSize;
    sun.security.provider.SHA3.SHAKE256 t1 = new sun.security.provider.SHA3.SHAKE256();

    private byte[] buffer;

    @Param({"500", "10000", "20000000"})
    private int bufferSize;
    
    @Setup
    public void setup() {
        Random rnd = new Random();
        buffer = new byte[bufferSize];
        rnd.nextBytes(buffer);
    }

    @Benchmark
    public void compress() {
        t1.implCompressTest(buffer, blockSize);
    }

    @Benchmark
    public void multiCompress() {
        t1.implMultiCompressTest(buffer, blockSize);
    }
}
