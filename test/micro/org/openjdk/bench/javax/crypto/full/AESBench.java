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

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;
import java.security.GeneralSecurityException;
import java.nio.ByteBuffer;

public class AESBench extends CryptoBase {

    public static final int SET_SIZE = 128;

    @Param({"AES/ECB/NoPadding", "AES/ECB/PKCS5Padding", "AES/CBC/NoPadding", "AES/CBC/PKCS5Padding"})
    private String algorithm;

    @Param({"128", "192", "256"})
    private int keyLength;

    @Param({"" + 16 * 1024})
    private int dataSize;

    public enum DataMethod{BYTE, DIRECT, HEAP}
    @Param({"BYTE", "DIRECT", "HEAP"})
    private DataMethod dataMethod;

    byte[][] data;
    byte[][] encryptedData;
    private Cipher encryptCipher;
    private Cipher decryptCipher;
    int index = 0;

    private ByteBuffer[] dataBuffer = new ByteBuffer[SET_SIZE];
    private ByteBuffer[] encryptedDataBuffer = new ByteBuffer[SET_SIZE];
    private ByteBuffer outBuffer;

    @Setup
    public void setup() throws GeneralSecurityException {
        setupProvider();
        byte[] keystring = fillSecureRandom(new byte[keyLength / 8]);
        SecretKeySpec ks = new SecretKeySpec(keystring, "AES");
        encryptCipher = makeCipher(prov, algorithm);
        encryptCipher.init(Cipher.ENCRYPT_MODE, ks);
        decryptCipher = makeCipher(prov, algorithm);
        decryptCipher.init(Cipher.DECRYPT_MODE, ks, encryptCipher.getParameters());
        data = fillRandom(new byte[SET_SIZE][dataSize]);
        encryptedData = fillEncrypted(data, encryptCipher);

        for (int i = 0; i < data.length; i++) {
            if (dataMethod == DataMethod.HEAP) {
                dataBuffer[i] = ByteBuffer.wrap(data[i]);
                encryptedDataBuffer[i] = ByteBuffer.wrap(encryptedData[i]);
                outBuffer = ByteBuffer.allocate(dataSize);
            } else if (dataMethod == DataMethod.DIRECT) {
                dataBuffer[i] = ByteBuffer.allocateDirect(data[i].length);
                dataBuffer[i].put(data[i]);
                encryptedDataBuffer[i] = ByteBuffer.allocateDirect(encryptedData[i].length);
                encryptedDataBuffer[i].put(encryptedData[i]);
                outBuffer = ByteBuffer.allocateDirect(dataSize);
            }
        }
    }

    @Benchmark
    public void encrypt(Blackhole bh) throws GeneralSecurityException {
        if (dataMethod == DataMethod.BYTE) {
            bh.consume(encryptCipher.doFinal(data[index]));
        } else {
            bh.consume(encryptCipher.doFinal(dataBuffer[index], outBuffer));
            dataBuffer[index].rewind();
            outBuffer.clear();
        }
        index = (index +1) % SET_SIZE;
    }

    @Benchmark
    public void decrypt(Blackhole bh) throws GeneralSecurityException {
        if (dataMethod == DataMethod.BYTE) {
            bh.consume(decryptCipher.doFinal(encryptedData[index]));
        } else {
            bh.consume(decryptCipher.doFinal(encryptedDataBuffer[index], outBuffer));
            encryptedDataBuffer[index].rewind();
            outBuffer.clear();
        }
        index = (index +1) % SET_SIZE;
    }

}
