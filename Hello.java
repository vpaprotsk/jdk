import sun.security.provider.*;
import java.nio.ByteOrder;
import java.lang.invoke.VarHandle;
import java.lang.invoke.MethodHandles;
import java.util.Random;

public class Hello {
    static java.util.HexFormat hex = java.util.HexFormat.of();
    public static void main(String[] args) throws Exception {
        // test();
        fuzzTest();
    }

    public static void test() {
        long[] state1 = new long[25];
        long[] state2 = new long[25];
        long[] state3 = new long[25];
        long[] state4 = new long[25];
        for (int i=0; i<state1.length; i++) {
            state1[i] = 0x100L+i;
            state2[i] = 0x200L+i;

            state3[i] = 0x100L+i;
            state4[i] = 0x200L+i;
        }
        long[] state5 = {
0x257921d1a26e5104L, 0x4901ca3ba6071a2cL, 0x1263d488dc44e050L, 0xa955f1030f188bb1L, 0x9a9b7544accee152L,
0x656c32bd05fdd778L, 0xb4909d316717fa30L, 0x476c4136f657dfc9L, 0x1c708abae2f5ad56L, 0x63b1db7d5ed8ab2dL,
0x1d98f1901036e96aL, 0x0eea9ddd24d4f46dL, 0xe09fca9df4d79957L, 0xd747d1e32f59ba92L, 0x8aef240fdf383446L,
0x6dc43e5d32c5d0e4L, 0xbcf4f154d5594d9fL, 0xac7df756d5e76eeeL, 0x3be6db178a1b250cL, 0x27242e1e999dc078L,
0xd41a0f4861813676L, 0xf27901422bfe758bL, 0xc0ef11114cf9ca04L, 0x8fe74c8c0fff5270L, 0x275188df88a98422L
    } ;long[] state6 = {
0x8004f88bcbd75e7eL, 0x6144afcc8d26e90aL, 0xb32a59ae69987966L, 0x59e30fcf1839c8b5L, 0x39c10343e9e5c47fL,
0xb533771724e93732L, 0xa2d38ac2f464009cL, 0xbb4fd4bb5068de73L, 0xee40cce1221dc4cdL, 0x2430070f4d5ec6a8L,
0xb35c3d9237a58e63L, 0x34042ba8da376643L, 0x7042380a6bdbd0f0L, 0xe2c2ef5e349db1f5L, 0x96e04ec12c590643L,
0x43ebde429b172caeL, 0x2cb19b69107bbe6eL, 0xa37db3abadc3f7a0L, 0x122f0ef7ec1aafc3L, 0x0d20aa2ebf42e23cL,
0x46f3c1a432dac6d2L, 0x3284cba166b97a32L, 0xbf9542e11d29366cL, 0xe5f44115e4001b74L, 0xa22fe9643fd987a2L
    };
        byte[] b = hex.parseHex(
            "00000000ff000000" +
            "0000000001000000" +
            "0000000002000000" +
            "0000000003000000" +
            "0000000004000000" +

            "0000000005000000" +
            "0000000006000000" +
            "0000000007000000" +
            "0000000008000000" +
            "0000000009000000" +

            "000000000a000000" +
            "000000000b000000" +
            "000000000c000000" +
            "000000000d000000" +
            "000000000e000000" +

            "0000000010000000" +
            "0000000011000000" +
            // "0000000012000000" +
            // "0000000013000000" +
            // "0000000014000000" +

            // "0000000015000000" +
            // "0000000016000000" +
            // "0000000017000000" +
            // "0000000018000000" +
            // "0000000019000000" +

            "0000ff0000000000" +
            "0000010000000000" +
            "0000020000000000" +
            "0000030000000000" +
            "0000040000000000" +

            "0000050000000000" +
            "0000060000000000" +
            "0000070000000000" +
            "0000080000000000" +
            "0000090000000000" +

            "00000a0000000000" +
            "00000b0000000000" +
            "00000c0000000000" +
            "00000d0000000000" +
            "00000e0000000000" +

            "0000100000000000" +
            "0000110000000000" 
            // "0000120000000000" +
            // "0000130000000000" +
            // "0000140000000000" +

            // "0000150000000000" 
            // "0000160000000000" +
            // "0000170000000000" +
            // "0000180000000000" +
            // "0000190000000000"

            // "0000ff0000000000" +
            // "0000010000000000" +
            // "0000020000000000" +
            // "0000030000000000" +
            // "0000040000000000" +

            // "0000050000000000" +
            // "0000060000000000" +
            // "0000070000000000" +
            // "0000080000000000" +
            // "0000090000000000" +

            // "00000a0000000000" +
            // "00000b0000000000" +
            // "00000c0000000000" +
            // "00000d0000000000" +
            // "00000e0000000000" +

            // "0000100000000000" +
            // "0000110000000000" +
            // "0000120000000000" +
            // "0000130000000000" +
            // "0000140000000000" +

            // "0000150000000000"
            // "0000160000000000" +
            // "0000170000000000" +
            // "0000180000000000" +
            // "0000190000000000"
            //   "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            // + "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            // + "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            // + "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            // + "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            );

        SHA3.SHAKE256 t1 = new SHA3.SHAKE256();
        // t1.testCompress(state1, b, 72);
        // t1.testMultiCompress(state1, b);

        // (5) 72, 104, 136, 144, 168
        // (3) single, multibuf, doublekeccak
        // (2) avx2, avx512
        // (2) linux, win
        // 60 variations

        // for (int i=0; i<1000; i++) {
        //     long[] st1 = state1.clone();
        //     // long[] st2 = state2.clone();
        //     // SHA3Parallel.Shake128Parallel.doubleKeccak(st1, st2);
        //     // t1.testCompress(st1, b, 72);
        //     t1.testMultiCompress(st1, b, 168);
        // // }
        //     // dumpState(st1);
        //     // System.out.println();
        //     // System.out.println();
        //     // dumpState(st2);
        //     // System.out.println("---");
        // }

        // for (int i=0; i<2; i++) {
        //     long[] st1 = new long[state1.length];
        //     for (int j = 0; j<st1.length; j++) {
        //         st1[j] = state1[j];
        //     }
        //     t1.testMultiCompress(st1, b, 168);
        //     dumpState(st1);
        // }

        // for (int i=0; i<2; i++) {
        //     t1.testMultiCompress(state1, b, 168);
        //     dumpState(state1);
        // }

        for (int i=0; i<200000; i++) {
            // t1.testCompress(state1, b, 168);
            // t1.testMultiCompress(state1, b, 136);
            // dumpState(t1.state);

            long[] state1Copy = state1.clone();
            long[] state2Copy = state2.clone();
            sun.security.provider.SHA3Parallel.doubleKeccak(state1Copy, state2Copy);
            if (state2Copy[0]!=0x7b9088601a60301bL) {
                dumpState(state1Copy);
                dumpState(state2Copy);
            }
            // implCompressMultiBlock(state1, 168, b, 0, b.length-168);
            // dumpState(state1);

            // SHA3Parallel.Shake128Parallel.doubleKeccak(state1, state2);
            // SHA3.keccak(state3);
            // SHA3.keccak(state4);
            // dumpState(state1);
            // dumpState(state3);
            // dumpState(state2);
            // dumpState(state4);
        }


        // t1.testMultiCompress(state1, buf, blockSize);
        // implCompressMultiBlock(state1, blockSize, buf, 0, buf.length-blockSize);
    }

    public static void dumpState(long[] state) {
        System.out.print(hex.toHexDigits(state[0]) + " ");
        for (int i=1; i<state.length; i++) {
            if (i%5 == 0) {
                System.out.println();
            }
            System.out.print(hex.toHexDigits(state[i]) + " ");
        }
        System.out.println();
        System.out.println("---");
    }


    static int implCompressMultiBlock(long[] state, int blockSize, byte[] b, int ofs, int limit) {
        for (; ofs <= limit; ofs += blockSize) {
            implCompress(state, blockSize, b, ofs);
        }
        return ofs;
    }

    static void implCompress(long[] state, int blockSize, byte[] b, int ofs) {
        for (int i = 0; i < blockSize / 8; i++) {
            state[i] ^= (long) asLittleEndian.get(b, ofs);
            ofs += 8;
        }

        SHA3.keccak(state);
    }

    static final VarHandle asLittleEndian
            = MethodHandles.byteArrayViewVarHandle(long[].class,
            ByteOrder.LITTLE_ENDIAN).withInvokeExactBehavior();

    static int[] blockSizes = {72, 104, 136, 144, 168};
    static void fuzzTest() {
        SHA3.SHAKE256 t1 = new SHA3.SHAKE256();
        Random rnd = new Random();
        long seed = 1; //rnd.nextLong();
        rnd.setSeed(seed);

        long[] state1 = new long[25];
        long[] state2 = new long[25];
        long[] state3 = new long[25];
        long[] state4 = new long[25];
        long[] state5 = new long[25];
        long[] state6 = new long[25];
        long[] state7 = new long[25];
        long[] state8 = new long[25];

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

        long[] state1Ref = state1.clone();
        long[] state2Ref = state2.clone();
        long[] state3Ref = state3.clone();
        long[] state4Ref = state4.clone();
        long[] state5Ref = state5.clone();
        long[] state6Ref = state6.clone();
        long[] state7Ref = state7.clone();
        long[] state8Ref = state8.clone();

        long[] state1copy1 = state1.clone();
        long[] state2copy1 = state2.clone();

        long[] state1copy2 = state1.clone();
        long[] state2copy2 = state2.clone();
        long[] state3copy2 = state3.clone();
        long[] state4copy2 = state4.clone();

        long[] state1copy3 = state1.clone();
        long[] state2copy3 = state2.clone();
        long[] state3copy3 = state3.clone();
        long[] state4copy3 = state4.clone();
        long[] state5copy3 = state5.clone();
        long[] state6copy3 = state6.clone();
        long[] state7copy3 = state7.clone();
        long[] state8copy3 = state8.clone();

        for (int i = 0; i<1000000; i++) {
            int len = blockSizes[rnd.nextInt(blockSizes.length)] * oneMany(rnd);// + zeroOneMany(rnd);
            // byte[] buf = new byte[len];
            // rnd.nextBytes(buf);

            for (int blockSize : blockSizes) {
                if (len<blockSize) continue;
                //int len = blockSize * oneMany(rnd);
                byte[] buf = new byte[len];
                rnd.nextBytes(buf);

                t1.testCompress(state1, buf, blockSize);
                implCompress(state1, blockSize, buf, 0);

                if (!java.util.Arrays.equals(state1, t1.state)) {
                    dumpState(t1.state);
                    dumpState(state1);
                    throw new RuntimeException("Fail " + i + "[blocksize "+blockSize+"] [length "+len+"]");
                }

                t1.testMultiCompress(state2, buf, blockSize);
                implCompressMultiBlock(state2, blockSize, buf, 0, buf.length-blockSize);

                if (!java.util.Arrays.equals(state2, t1.state)) {
                    dumpState(t1.state);
                    dumpState(state2);
                    throw new RuntimeException("Fail testMultiCompress " + i + "[blocksize "+blockSize+"] [length "+len+"]");
                }
            }
            
            SHA3.keccak(state1Ref);
            SHA3.keccak(state2Ref);
            SHA3.keccak(state3Ref);
            SHA3.keccak(state4Ref);
            SHA3.keccak(state5Ref);
            SHA3.keccak(state6Ref);
            SHA3.keccak(state7Ref);
            SHA3.keccak(state8Ref);

            sun.security.provider.SHA3Parallel.doubleKeccak(state1copy1, state2copy1);

            if (!java.util.Arrays.equals(state1copy1, state1Ref)) {
                dumpState(state1copy1);
                dumpState(state1Ref);
                throw new RuntimeException("Fail doubleKeccak low " + i);
            }
            if (!java.util.Arrays.equals(state2copy1, state2Ref)) {
                dumpState(state2copy1);
                dumpState(state2Ref);
                throw new RuntimeException("Fail doubleKeccak high " + i);
            }

            sun.security.provider.SHA3Parallel.quadKeccak(state1copy2, state2copy2, state3copy2, state4copy2);

            if (!java.util.Arrays.equals(state1copy2, state1Ref)) {
                dumpState(state1copy2);
                dumpState(state1Ref);
                throw new RuntimeException("Fail quadKeccak low " + i);
            }
            if (!java.util.Arrays.equals(state2copy2, state2Ref)) {
                dumpState(state2copy2);
                dumpState(state2Ref);
                throw new RuntimeException("Fail quadKeccak high " + i);
            }
            if (!java.util.Arrays.equals(state3copy2, state3Ref)) {
                dumpState(state3copy2);
                dumpState(state3Ref);
                throw new RuntimeException("Fail quadKeccak 3 " + i);
            }
            if (!java.util.Arrays.equals(state4copy2, state4Ref)) {
                dumpState(state4copy2);
                dumpState(state4Ref);
                throw new RuntimeException("Fail quadKeccak 4 " + i);
            }

            sun.security.provider.SHA3Parallel.eightKeccak(state1copy3, state2copy3, 
                state3copy3, state4copy3, state5copy3, state6copy3, state7copy3, state8copy3);

            if (!java.util.Arrays.equals(state1copy3, state1Ref)) {
                dumpState(state1copy3);
                dumpState(state1Ref);
                throw new RuntimeException("Fail eightKeccak low " + i);
            }
            if (!java.util.Arrays.equals(state2copy3, state2Ref)) {
                dumpState(state2copy3);
                dumpState(state2Ref);
                throw new RuntimeException("Fail eightKeccak high " + i);
            }
            if (!java.util.Arrays.equals(state3copy3, state3Ref)) {
                dumpState(state3copy3);
                dumpState(state3Ref);
                throw new RuntimeException("Fail eightKeccak 3 " + i);
            }
            if (!java.util.Arrays.equals(state4copy3, state4Ref)) {
                dumpState(state4copy3);
                dumpState(state4Ref);
                throw new RuntimeException("Fail eightKeccak 4 " + i);
            }
            if (!java.util.Arrays.equals(state5copy3, state5Ref)) {
                dumpState(state5copy3);
                dumpState(state5Ref);
                throw new RuntimeException("Fail eightKeccak 5 " + i);
            }
            if (!java.util.Arrays.equals(state6copy3, state6Ref)) {
                dumpState(state6copy3);
                dumpState(state6Ref);
                throw new RuntimeException("Fail eightKeccak 6 " + i);
            }
            if (!java.util.Arrays.equals(state7copy3, state7Ref)) {
                dumpState(state7copy3);
                dumpState(state7Ref);
                throw new RuntimeException("Fail eightKeccak 7 " + i);
            }
            if (!java.util.Arrays.equals(state8copy3, state8Ref)) {
                dumpState(state8copy3);
                dumpState(state8Ref);
                throw new RuntimeException("Fail eightKeccak 8 " + i);
            }

            if (i%10000 == 0) {
                System.out.print('.');
            }
        }
        System.out.println("Passed");

        // - Z multiple of X blocksize + Y offset
    }

    static int zeroOneMany(Random rnd) {
        switch(rnd.nextInt(3)) {
            case 0: return 0;
            case 1: return 1;
            default: return rnd.nextInt(100)+1;
        }
    }

    static int oneMany(Random rnd) {
        switch(rnd.nextInt(2)) {
            case 0: return 1;
            default: return rnd.nextInt(100)+1;
        }
    }
}

// ./build/linux-x86_64-server-fastdebug/images/jdk/bin/java --add-exports java.base/sun.security.provider=ALL-UNNAMED -XX:+UnlockDiagnosticVMOptions Hello.java