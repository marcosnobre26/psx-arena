#!/bin/sh
# Instala o PSn00bSDK no Ubuntu/Debian/WSL usando o compilador MIPS do sistema.
# Use isto se NÃO quiser Docker. Depois coloque USE_DOCKER=0 no dev.conf
# e compile normalmente com ./dev build
set -e
PREFIX=${PREFIX:-/opt/psn00bsdk}
COMMIT=5d9aa2d3dfc7d6e51c2eb942ab4cdbae5571a40a

sudo apt-get update
sudo apt-get install -y git cmake ninja-build build-essential python3 python3-pil \
    gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu

TMP=$(mktemp -d)
git clone --recurse-submodules https://github.com/Lameguy64/PSn00bSDK "$TMP/psn00bsdk"
cd "$TMP/psn00bsdk"
git checkout $COMMIT
git submodule update --init --recursive
cmake --preset default -DPSN00BSDK_TARGET=mipsel-linux-gnu -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build ./build || true      # exemplos C++ do SDK falham; as bibliotecas compilam
sudo cmake --install ./build
rm -rf "$TMP"

echo
echo "PSn00bSDK instalado em $PREFIX."
echo "Agora coloque USE_DOCKER=0 no arquivo dev.conf e rode ./dev build"
