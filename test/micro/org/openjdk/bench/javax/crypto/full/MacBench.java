/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.nio.ByteBuffer;

public class MacBench extends CryptoBase {

    public static final int SET_SIZE = 128;

    @Param({"HmacMD5", "HmacSHA1", "HmacSHA256", "HmacSHA512"})
    private String algorithm;

    @Param({"128", "1024"})
    int dataSize;

    private byte[][] data;
    private ByteBuffer[] dataBuffer = new ByteBuffer[SET_SIZE];
    private Mac mac;
    int index = 0;

    public enum DataMethod{BYTE, DIRECT, HEAP}
    @Param({"BYTE", "DIRECT", "HEAP"})
    private DataMethod dataMethod;

    @Setup
    public void setup() throws NoSuchAlgorithmException, InvalidKeyException {
        setupProvider();
        mac = (prov == null) ? Mac.getInstance(algorithm) : Mac.getInstance(algorithm, prov);
        mac.init(KeyGenerator.getInstance(algorithm).generateKey());
        data = fillRandom(new byte[SET_SIZE][dataSize]);

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
    public void mac(Blackhole bh) throws NoSuchAlgorithmException {
        if (dataMethod == DataMethod.BYTE) {
            bh.consume(mac.doFinal(data[index]));
        } else {
            mac.update(dataBuffer[index]);
            bh.consume(mac.doFinal());
            dataBuffer[index].rewind();
        }
        index = (index + 1) % SET_SIZE;
    }

}
