/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

import javax.crypto.*;
import javax.crypto.spec.*;
import java.nio.ByteBuffer;
import java.security.*;
import java.security.spec.*;


public abstract class CipherBench extends CryptoBase {

    @Param({})
    private String permutation;

    @Param({})
    private String mode;

    @Param({})
    private String padding;

    @Param({})
    private int keyLength;

    @Param({})
    private int dataSize;

    public enum DataMethod{BYTE, DIRECT, HEAP}
    @Param({"BYTE", "DIRECT", "HEAP"})
    private DataMethod dataMethod;

    private int decryptCount = 0;
    private byte[] data;
    private byte[][] encryptedData = new byte[2][];
    private byte[] outBuffer;
    private Cipher[] encryptCipher = new Cipher[2];
    private Cipher decryptCipher;
    protected SecretKeySpec ks;
    protected byte[] iv;

    private ByteBuffer dataBuffer;
    private ByteBuffer[] encryptedDataBuffer = new ByteBuffer[2];
    private ByteBuffer outBuffer2;

    protected abstract int ivLength();
    protected abstract AlgorithmParameterSpec makeParameterSpec();

    protected void init(Cipher c, int mode, SecretKeySpec ks)
        throws GeneralSecurityException {

        if (iv == null) {
            iv = fillSecureRandom(new byte[ivLength()]);
        }

        // toggle some bits in the IV to get around IV reuse defenses
        iv[0] ^= (byte)0xFF;
        AlgorithmParameterSpec paramSpec = makeParameterSpec();

        c.init(mode, ks, paramSpec);
    }

    protected void init(Cipher c, int mode, SecretKeySpec ks, Cipher fromCipher)
        throws GeneralSecurityException {

        AlgorithmParameters params = fromCipher.getParameters();
        c.init(mode, ks, fromCipher.getParameters());
    }

    @Setup
    public void setup() throws GeneralSecurityException {
        setupProvider();

        String transform = permutation + "/" + mode + "/" + padding;
        byte[] keystring = fillSecureRandom(new byte[keyLength / 8]);
        ks = new SecretKeySpec(keystring, permutation);
        data = fillRandom(new byte[dataSize]);
        for (int i = 0; i < 2; i++) {
            encryptCipher[i] = makeCipher(prov, transform);
            init(encryptCipher[i], Cipher.ENCRYPT_MODE, ks);
            encryptedData[i] = encryptCipher[i].doFinal(data);
        }
        outBuffer = new byte[dataSize + 128]; // extra space for tag, etc
        decryptCipher = makeCipher(prov, transform);

        if (dataMethod == DataMethod.HEAP) {
            dataBuffer = ByteBuffer.wrap(data);
            for (int i = 0; i < 2; i++) {
                encryptedDataBuffer[i] = ByteBuffer.wrap(encryptedData[i]);
            }
            outBuffer2 = ByteBuffer.allocate(outBuffer.length);
        } else if (dataMethod == DataMethod.DIRECT) {
            dataBuffer = ByteBuffer.allocateDirect(data.length);
            dataBuffer.put(data);
            for (int i = 0; i < 2; i++) {
                encryptedDataBuffer[i] = ByteBuffer.allocateDirect(encryptedData[i].length);
                encryptedDataBuffer[i].put(encryptedData[i]);
            }
            outBuffer2 = ByteBuffer.allocateDirect(outBuffer.length);
        }
    }

    @Benchmark
    public void encrypt(Blackhole bh) throws GeneralSecurityException {
        init(encryptCipher[1], Cipher.ENCRYPT_MODE, ks);
        if (dataMethod == DataMethod.BYTE) {
            encryptCipher[1].doFinal(data, 0, data.length, outBuffer);
        } else {
            encryptCipher[1].doFinal(dataBuffer, outBuffer2);
            dataBuffer.rewind();
            outBuffer2.clear();
        }

    }

    @Benchmark
    public void decrypt(Blackhole bh) throws GeneralSecurityException {
        init(decryptCipher, Cipher.DECRYPT_MODE, ks,
            encryptCipher[decryptCount]);
        if (dataMethod == DataMethod.BYTE) {
            bh.consume(
                decryptCipher.doFinal(encryptedData[decryptCount], 0,
                    encryptedData[decryptCount].length, outBuffer));
        } else {
            bh.consume(encryptCipher[1].doFinal(dataBuffer, outBuffer2));
            dataBuffer.rewind();
            outBuffer2.clear();
        }
        decryptCount = (decryptCount + 1) % 2;
    }

    public static class GCM extends CipherBench {

        @Param({"AES"})
        private String permutation;

        @Param({"GCM"})
        private String mode;

        @Param({"NoPadding"})
        private String padding;

        @Param({"128", "256"})
        private int keyLength;

        @Param({"1024", "" + 16 * 1024})
        private int dataSize;

        protected int ivLength() {
            return 32;
        }
        protected AlgorithmParameterSpec makeParameterSpec() {
            return new GCMParameterSpec(96, iv, 0, 16);
        }

        private byte[] aad;

        protected void init(Cipher c, int mode, SecretKeySpec ks)
            throws GeneralSecurityException {

            if (aad == null) {
                aad = fillSecureRandom(new byte[5]);
            }

            super.init(c, mode, ks);
            c.updateAAD(aad);
        }

        protected void init(Cipher c, int mode, SecretKeySpec ks,
            Cipher fromCipher) throws GeneralSecurityException {

            super.init(c, mode, ks, fromCipher);
            c.updateAAD(aad);
        }
    }

    public static class CTR extends CipherBench {

        @Param({"AES"})
        private String permutation;

        @Param({"CTR"})
        private String mode;

        @Param({"NoPadding"})
        private String padding;

        @Param({"128", "256"})
        private int keyLength;

        @Param({"1024", "" + 16 * 1024})
        private int dataSize;

        protected int ivLength() {
            return 16;
        }
        protected AlgorithmParameterSpec makeParameterSpec() {
            return new IvParameterSpec(iv);
        }
    }

    public static class ChaCha20Poly1305 extends CipherBench {

        @Param({"ChaCha20-Poly1305"})
        private String permutation;

        @Param({"None"})
        private String mode;

        @Param({"NoPadding"})
        private String padding;

        @Param({"256"})
        private int keyLength;

        @Param({"256", "1024", "4096", "16384"})
        private int dataSize;

        protected int ivLength() {
            return 12;
        }
        protected AlgorithmParameterSpec makeParameterSpec() {
            return new IvParameterSpec(iv);
        }
    }

    public static class ChaCha20 extends CipherBench {

        @Param({"ChaCha20"})
        private String permutation;

        @Param({"None"})
        private String mode;

        @Param({"NoPadding"})
        private String padding;

        @Param({"256"})
        private int keyLength;

        @Param({"256", "1024", "4096", "16384"})
        private int dataSize;

        protected int ivLength() {
            return 12;
        }
        protected AlgorithmParameterSpec makeParameterSpec() {
            return new ChaCha20ParameterSpec(iv, 0);
        }

        protected void init(Cipher c, int mode, SecretKeySpec ks,
            Cipher fromCipher) throws GeneralSecurityException {

            AlgorithmParameterSpec paramSpec =
                new ChaCha20ParameterSpec(fromCipher.getIV(), 0);
            c.init(mode, ks, paramSpec);
        }
    }
}
