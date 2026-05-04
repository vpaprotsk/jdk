import java.math.BigInteger;
import java.util.Arrays;
import java.util.Random;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public class Hello {
    static java.util.HexFormat hex = java.util.HexFormat.of();

    public static void main(String[] args) throws Exception {
        if (false) {
            for (int i=0; i<3; i++) {
                //foo();
                foo2();
            }
        } else {
            for (int i = 0; true; i++) {
                foo3();
                if (i%1024*1024==0){
                    System.out.print(',');
                }
            }
        }
    }

    public static void foo() throws Exception {
        Random rnd = new Random();
        long seed = -2300717692259729805L; //rnd.nextLong();
        rnd.setSeed(seed);

        byte[] keyBytes = new byte[] { 
            1,  2,  3,  4,  5,  6,  7,  8, 
            9, 10, 11, 12, 13, 14, 15, 16 
        };
        byte[] ivBytes = new byte[] {
            11, 12, 13, 14, 15, 16, 17, 18, 
            19, 20, 21, 22, 23, 24, 25, 26
        };
        byte[] inputData //= "Hello, AES CTR Mode!".getBytes();
         = hex.parseHex("88a127e1a012db34");

        SecretKeySpec secretKey = new SecretKeySpec(keyBytes, "AES");
        IvParameterSpec ivSpec = new IvParameterSpec(ivBytes);
        byte[] outputData1 = new byte[inputData.length];
        byte[] outputData2 = new byte[inputData.length];

        // Cipher encryptCipher = Cipher.getInstance("AES/CTR/NoPadding");
        // encryptCipher.init(Cipher.ENCRYPT_MODE, secretKey, ivSpec);
        
        com.sun.crypto.provider.CounterMode cipher = 
            new com.sun.crypto.provider.CounterMode(new com.sun.crypto.provider.AES_Crypt());
        cipher.init(true, "AES", keyBytes, ivBytes);


        //for (int i=0; i<10000; i++) {
        //while (true) {
            cipher.implCrypt(inputData, 0, inputData.length, outputData1, 0);
            cipher.reset();
        //}
        cipher.implCryptJava(inputData, 0, inputData.length, outputData2, 0);

        // byte[] encryptedData = encryptCipher.doFinal(inputData);
        System.out.println();System.out.println();
        // System.out.println("Reference1: " + hex.formatHex(encryptedData));
        System.out.println("Reference: " + hex.formatHex(outputData2));
        System.out.println("Encrypted: " + hex.formatHex(outputData1));
    }

    public static void foo3() throws Exception {
        Random rnd = new Random();
        long seed = rnd.nextLong();
        rnd.setSeed(seed);
        int keySize = 128 + rnd.nextInt(3) * 64;
        byte[] key = new byte[keySize/8];
        byte[] iv = new byte[16];
        rnd.nextBytes(iv);
        rnd.nextBytes(key);

        // System.out.println("keySize: " + keySize);

        if (rnd.nextBoolean()) { // carry
            for (int i=15; i>8; i--) {
                iv[i] = (byte)0xFF;
            }
            iv[8] = (byte)(0xFF-rnd.nextInt(32));
        }

        com.sun.crypto.provider.CounterMode cipher = 
            new com.sun.crypto.provider.CounterMode(new com.sun.crypto.provider.AES_Crypt());
        cipher.init(true, "AES", key, iv);

        com.sun.crypto.provider.CounterMode cipherRef = 
            new com.sun.crypto.provider.CounterMode(new com.sun.crypto.provider.AES_Crypt());
        cipherRef.init(true, "AES", key, iv);

        int iterations = rnd.nextInt(20); // streaming
        for (int i=0; i<iterations; i++) {
            int payloadSize = rnd.nextInt(4*4*16+20);
            if (payloadSize>4*4*16) {
                payloadSize = rnd.nextInt(10240);
            }
            byte[] inputData = new byte[payloadSize];
            byte[] outputData1 = new byte[payloadSize+32];
            byte[] outputData2 = new byte[payloadSize+32];
            rnd.nextBytes(inputData);
            cipher.implCrypt(inputData, 0, inputData.length, outputData1, 0);
            cipherRef.implCryptJava(inputData, 0, inputData.length, outputData2, 0);

            if (!Arrays.equals(outputData1, outputData2)) {
                System.out.println("Seed1:     " + seed);
                System.out.println("Length:    " + inputData.length);
                System.out.println("Reference: " + hex.formatHex(outputData2));
                System.out.println("Encrypted: " + hex.formatHex(outputData1));
                System.out.println("Input:     " + hex.formatHex(inputData));
                throw new RuntimeException("Iteration:      " + i);
            }
            int diff = cipher.used - cipherRef.used;
            if ( !(diff == 0 || diff == 16 || diff == -16)) {
                System.out.println("Seed2:     " + seed);
                System.out.println("Length:    " + inputData.length);
                System.out.println("Reference: " + cipherRef.used);
                System.out.println("Used:      " + cipher.used);
                System.out.println("Input:     " + hex.formatHex(inputData));
                throw new RuntimeException("Iteration:      " + i);
            }
            if (cipher.used<16 && !Arrays.equals(cipher.encryptedCounter, cipherRef.encryptedCounter)) {
                System.out.println("Seed3:     " + seed);
                System.out.println("Length:    " + inputData.length);
                System.out.println("Reference: " + hex.formatHex(cipherRef.encryptedCounter));
                System.out.println("Counter:   " + hex.formatHex(cipher.encryptedCounter));
                System.out.println("Input:     " + hex.formatHex(inputData));
                throw new RuntimeException("Iteration:      " + i);
            }
        }
    }

    public static void foo2() throws Exception {
        Random rnd = new Random();
        long seed = rnd.nextLong();
        rnd.setSeed(seed);

        byte[] keyBytes = new byte[] { 
            1,  2,  3,  4,  5,  6,  7,  8, 
            9, 10, 11, 12, 13, 14, 15, 16,
            1,  2,  3,  4,  5,  6,  7,  8, 
            9, 10, 11, 12, 13, 14, 15, 16 
        };
        byte[] ivBytes = new byte[] {
            11, 12, 13, 14, 15, 16, 17, 18, 
            19, 20, 21, 22, 23, 24, 25, 26
        };
        byte[] inputData;

        //SecretKeySpec secretKey = new SecretKeySpec(keyBytes, "AES");
        IvParameterSpec ivSpec = new IvParameterSpec(ivBytes);
        com.sun.crypto.provider.CounterMode cipher = 
            new com.sun.crypto.provider.CounterMode(new com.sun.crypto.provider.AES_Crypt());
        cipher.init(true, "AES", keyBytes, ivBytes);


        for (int i=0; true || i<10000; i++) {
            inputData = new byte[32]; //[rnd.nextInt(1024)];
            byte[] outputData1 = new byte[2*inputData.length];
            byte[] outputData2 = new byte[2*inputData.length];

            rnd.nextBytes(inputData);
            cipher.implCrypt(inputData, 0, inputData.length, outputData1, 0);
            cipher.reset();
            cipher.implCryptJava(inputData, 0, inputData.length, outputData2, 0);
            cipher.reset();
            if (!Arrays.equals(outputData1, outputData2)) {
                System.out.println("Seed:      " + seed);
                System.out.println("Length:    " + inputData.length);
                System.out.println("Reference: " + hex.formatHex(outputData2));
                System.out.println("Encrypted: " + hex.formatHex(outputData1));
                System.out.println("Input:     " + hex.formatHex(inputData));
                break;
            } else if (i%1024*1024==0){
                System.out.print(',');
            }
        }
    }
}
