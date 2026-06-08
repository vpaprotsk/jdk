import sun.security.provider.ML_DSA;

class Hello {
    static final int ML_DSA_44 = 2;
    static final int ML_DSA_65 = 3;
    static final int ML_DSA_87 = 5;
    static final int SEED_LEN = 32;
    public static void main(String[] args) {
        ML_DSA mlDsa = new ML_DSA(ML_DSA_87);
        byte[] seed = new byte[SEED_LEN];
        ML_DSA.ML_DSA_KeyPair kp = mlDsa.generateKeyPairInternal(seed);
    }
}