#!/usr/bin/env bash
# Prepare the NUCLEO-C5A3ZG PQC OTA demo:
#   1. configure + build wolfBoot (ML-DSA-44, or ML-DSA-44 + ECC P-256 hybrid)
#   2. build the OTA application
#   3. sign it as v1, v2 and v3, and make a tampered copy of v3
#   4. write demo/out/flash_cmds.txt
#
# Environment: WOLFBOOT_ROOT WOLFSSL_ROOT WOLFIP_ROOT (as for the Makefile),
#              HYBRID=1 for the hybrid signature, BENCH=1 for the handshake
#              benchmark at boot, STLINK_SN to pin the probe.
set -e
DEMO=$(cd "$(dirname "$0")/.." && pwd)
OUT=$DEMO/demo/out
WOLFBOOT_ROOT=${WOLFBOOT_ROOT:-$DEMO/../../wolfboot}
STLINK_SN=${STLINK_SN:-<ST-LINK serial>}
mkdir -p "$OUT"

echo "== wolfBoot (WOLFBOOT_ROOT=$WOLFBOOT_ROOT)"
cd "$WOLFBOOT_ROOT"
cp config/examples/stm32c5.config .config
sed -i '/^SIGN?=/d' .config
cat >> .config <<EOF
SIGN=ML_DSA
ML_DSA_LEVEL=2
IMAGE_SIGNATURE_SIZE=2420
IMAGE_HEADER_SIZE=8192
EOF
SIGN_ARGS="--ml_dsa --sha256"
KEYS="wolfboot_signing_private_key.der"
if [ "${HYBRID:-0}" = "1" ]; then
    printf 'SIGN_SECONDARY=ECC256\nWOLFBOOT_UNIVERSAL_KEYSTORE=1\n' >> .config
    SIGN_ARGS="--ml_dsa --ecc256 --sha256"
    KEYS="$KEYS wolfboot_signing_second_private_key.der"
fi
make wolfboot.bin
cp wolfboot.bin "$OUT/"

echo "== OTA application"
make -C "$DEMO" clean
make -C "$DEMO" BENCH="${BENCH:-0}" WOLFBOOT_ROOT="$WOLFBOOT_ROOT"
cp "$DEMO/build/app.bin" "$OUT/app.bin"

echo "== sign v1 v2 v3"
for v in 1 2 3; do
    ML_DSA_LEVEL=2 IMAGE_HEADER_SIZE=8192 ./tools/keytools/sign $SIGN_ARGS \
        "$OUT/app.bin" $KEYS $v > /dev/null
done

echo "== tampered v3 (one payload byte flipped after signing)"
cp "$OUT/app_v3_signed.bin" "$OUT/app_v3_tampered.bin"
python3 - "$OUT/app_v3_tampered.bin" <<'EOF'
import sys
p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
off = 8192 + 0x1000
b[off] ^= 0x01
open(p, 'wb').write(b)
print("flipped byte at offset 0x%x" % off)
EOF

cat > "$OUT/flash_cmds.txt" <<EOF
# Factory image: wolfBoot + signed v1 (full chip erase first)
STM32_Programmer_CLI -c port=swd sn=$STLINK_SN mode=UR -e all \\
    -d $OUT/wolfboot.bin 0x08000000 \\
    -d $OUT/app_v1_signed.bin 0x08010000 -v -rst

# Stage v2 directly into UPDATE over SWD (no network), then trigger from the app
STM32_Programmer_CLI -c port=swd sn=$STLINK_SN mode=UR \\
    -d $OUT/app_v2_signed.bin 0x08080000 -v -rst

# OTA server (host): serve v2, then v3, then the tampered v3, then v1 (downgrade)
$DEMO/host/ota_server -c $DEMO/certs -f $OUT/app_v2_signed.bin
EOF
ls -l "$OUT"
echo "Flash commands: $OUT/flash_cmds.txt"
