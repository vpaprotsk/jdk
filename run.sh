
# parallel vector enabled
arr=(
     "20"  "10" "100" # existing impls
    "421" "411" "401" # parallel = 4, all vector sizes
    "221" "211" "201" # parallel = 2, all vector sizes
    "621" "611" "601" # parallel = 6, all vector sizes
)

echo "starting run" > completelog.log
for s in "${arr[@]}"; do
    make install-hsdis test TEST="micro:org.openjdk.bench.javax.crypto.full.AESBench.encryptCTR" MICRO="JAVA_OPTIONS=-XX:+UnlockDiagnosticVMOptions -XX:DevAESCTR=$s;FORK=1;ITER=3;TIME=10;WARMUP_ITER=12;WARMUP_TIME=10;OPTIONS=-prof perfasm -p algorithm=AES/CTR/NoPadding" 2>&1 | tee -a completelog.log
done

