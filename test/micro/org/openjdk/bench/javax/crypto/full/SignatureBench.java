/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.security.SignatureException;
import java.util.Random;
import java.nio.ByteBuffer;

public class SignatureBench extends CryptoBase {

    public static final int SET_SIZE = 128;

    @Param({"SHA256withDSA"})
    private String algorithm;

    @Param({"1024", "16384"})
    int dataSize;

    @Param({"1024"})
    private int keyLength;

    public enum DataMethod{BYTE, DIRECT, HEAP}
    @Param({"BYTE", "DIRECT", "HEAP"})
    private DataMethod dataMethod;

    private PrivateKey privateKey;
    private PublicKey publicKey;
    private byte[][] data;
    private byte[][] signedData;
    int index;
    private ByteBuffer[] dataBuffer = new ByteBuffer[SET_SIZE];
    private Signature signature;
    private Signature signatureVerify;

    private String getKeyPairGeneratorName() {
        int withIndex = algorithm.lastIndexOf("with");
        if (withIndex < 0) {
            return algorithm;
        }
        String tail = algorithm.substring(withIndex + 4);
        return "ECDSA".equals(tail) ? "EC" : tail;
    }

    @Setup()
    public void setup() throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
        setupProvider();
        KeyPairGenerator kpg = KeyPairGenerator.getInstance(getKeyPairGeneratorName());
        kpg.initialize(keyLength);
        KeyPair keys = kpg.generateKeyPair();
        this.privateKey = keys.getPrivate();
        this.publicKey = keys.getPublic();
        signature = (prov == null) ? Signature.getInstance(algorithm) : Signature.getInstance(algorithm, prov);
        signature.initSign(privateKey);
        signatureVerify = (prov == null) ? Signature.getInstance(algorithm) : Signature.getInstance(algorithm, prov);
        signatureVerify.initVerify(publicKey);

        data = fillRandom(new byte[SET_SIZE][dataSize]);
        signedData = new byte[data.length][];
        for (int i = 0; i < data.length; i++) {
            signedData[i] = sign(data[i]);
        }

        for (int i = 0; i < data.length; i++) {
            if (dataMethod == DataMethod.HEAP) {
                dataBuffer[i] = ByteBuffer.wrap(data[i]);
            } else if (dataMethod == DataMethod.DIRECT) {
                dataBuffer[i] = ByteBuffer.allocateDirect(data[i].length);
                dataBuffer[i].put(data[i]);
            }
        }
    }

    public byte[] sign(byte[] data) throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
        signature.update(data);
        return signature.sign();
    }

    @Benchmark
    public void sign(Blackhole bh) throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
        if (dataMethod == DataMethod.BYTE) {
            signature.update(data[index]);
        } else {
            signature.update(dataBuffer[index]);
            dataBuffer[index].rewind();
        }
        bh.consume(signature.sign());
        index = (index + 1) % SET_SIZE;
    }

    @Benchmark
    public void verify(Blackhole bh) throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
        if (dataMethod == DataMethod.BYTE) {
            signature.update(data[index]);
        } else {
            signature.update(dataBuffer[index]);
            dataBuffer[index].rewind();
        }
        bh.consume(signature.verify(signedData[index]));
        index = (index + 1) % SET_SIZE;
    }

    public static class RSA extends SignatureBench {

        @Param({"MD5withRSA", "SHA256withRSA"})
        private String algorithm;

        @Param({"1024", "2048", "3072"})
        private int keyLength;

    }

    public static class ECDSA extends SignatureBench {

        @Param({"SHA256withECDSA"})
        private String algorithm;

        @Param({"256"})
        private int keyLength;

    }

    public static class EdDSA extends SignatureBench {

        @Param({"EdDSA"})
        private String algorithm;

        @Param({"255", "448"})
        private int keyLength;

    }

}
