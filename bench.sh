# $JAVA1 -XX:+UnlockDiagnosticVMOptions -XX:+DebugNonSafepoints -jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 6 -r 100000 -w 5 -p montgomery=true SignatureBench.ECDSA.sign
# ECDSA_CMD="-XX:DisableIntrinsic=_intpoly_montgomeryReduce_P256 -jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 100000 -w 5 -p montgomery=true SignatureBench.ECDSA.sign"
# ECDHE_CMD="-jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 10 -w 5 -p montgomery=true KeyAgreementBench.EC"
# ECMUL_CMD="-XX:CompileCommand=dontinline,sun/security/util/math/intpoly/MontgomeryIntegerPolynomialP256.conditionalAssign -jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 10 -w 5 -p isMontBench=true PolynomialP256Bench.benchAssign"
# ECRED_CMD="-XX:CompileCommand=dontinline,sun/security/util/math/intpoly/MontgomeryIntegerPolynomialP256.reduce -jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 100000 -w 5 -p isMontBench=true PolynomialP256Bench.benchReduce"

# JAVA2="./build/linux-x86_64-server-fastdebug/images/jdk/bin/java -XX:+UnlockDiagnosticVMOptions -XX:+UseIntPolyIntrinsics -XX:CompileCommand=dontinline,sun/security/util/math/intpoly/IntegerPolynomial.conditionalAssign"
# JAVA2="./build/linux-x86_64-server-fastdebug/images/jdk/bin/java -XX:+UnlockDiagnosticVMOptions -XX:+UseIntPolyIntrinsics"
#    -XX:DisableIntrinsic=_intpoly_montgomeryAssign_P256
#  
# $JAVA1 -XX:+UnlockDiagnosticVMOptions -XX:+DebugNonSafepoints -jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 6 -r 100000 -w 5 -p montgomery=true SignatureBench.ECDSA.sign

JAVAB=./build/linux-x86_64-server-fastdebug/base_image/jdk/bin/java
JAVA1="./build/linux-x86_64-server-fastdebug/images/jdk/bin/java -XX:+UnlockDiagnosticVMOptions -XX:-UseIntPolyIntrinsics"
JAVA2="./build/linux-x86_64-server-fastdebug/images/jdk/bin/java -XX:+UnlockDiagnosticVMOptions -XX:+UseIntPolyIntrinsics"
ECDSA_CMD="-jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 10 -w 5 SignatureBench.ECDSA"
ECDHE_CMD="-jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 10 -w 5 -p keyLength=256 KeyAgreementBench.EC"
ECMUL_CMD="-jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 10 -w 5 PolynomialP256Bench.benchMultiply"
XDH_CMD="-jar ./build/linux-x86_64-server-fastdebug/images/test/micro/benchmarks.jar -f 1 -i 3 -wi 5 -r 10 -w 5 org.openjdk.bench.javax.crypto.small.KeyAgreementBench.XDH"

echo $JAVAB $ECDSA_CMD
$JAVAB $ECDSA_CMD
echo $JAVA1 $ECDSA_CMD
$JAVA1 $ECDSA_CMD
echo $JAVA2 $ECDSA_CMD
$JAVA2 $ECDSA_CMD

echo $JAVAB $ECDHE_CMD
$JAVAB $ECDHE_CMD
echo $JAVA1 $ECDHE_CMD
$JAVA1 $ECDHE_CMD
echo $JAVA2 $ECDHE_CMD
$JAVA2 $ECDHE_CMD

echo $JAVAB $ECMUL_CMD
$JAVAB $ECMUL_CMD
echo $JAVA1 $ECMUL_CMD
$JAVA1 $ECMUL_CMD
echo $JAVA2 $ECMUL_CMD
$JAVA2 $ECMUL_CMD

echo $JAVAB $XDH_CMD
$JAVAB $XDH_CMD
echo $JAVA1 $XDH_CMD
$JAVA1 $XDH_CMD
echo $JAVA2 $XDH_CMD
$JAVA2 $XDH_CMD

# ecc-montgomery