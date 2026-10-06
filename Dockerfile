# Ambiente de compilação do PS1 Arena. Normalmente você não usa isto direto:
# o script ./dev cria a imagem e roda a compilação dentro dela.
#
#   ./dev image      (re)cria a imagem
#   ./dev build      compila
#
# Usa o compilador MIPS do Ubuntu (mipsel-linux-gnu) e compila o PSn00bSDK
# a partir do código-fonte, na versão testada com este projeto.
FROM ubuntu:24.04

ARG PSN00BSDK_COMMIT=5d9aa2d3dfc7d6e51c2eb942ab4cdbae5571a40a

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        git ca-certificates cmake ninja-build build-essential python3 python3-pil \
        gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

# Os exemplos em C++ do SDK não compilam com este compilador (por isso o
# "|| true"), mas as bibliotecas e ferramentas sim, e são só o que usamos.
RUN git clone --recurse-submodules https://github.com/Lameguy64/PSn00bSDK /tmp/psn00bsdk \
    && cd /tmp/psn00bsdk && git checkout ${PSN00BSDK_COMMIT} && git submodule update --init --recursive \
    && cmake --preset default -DPSN00BSDK_TARGET=mipsel-linux-gnu -DCMAKE_INSTALL_PREFIX=/opt/psn00bsdk \
    && (cmake --build ./build || true) \
    && cmake --install ./build \
    && test -f /opt/psn00bsdk/lib/libpsn00b/release/libpsxgpu_exe_gprel.a \
    && rm -rf /tmp/psn00bsdk

ENV PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b
ENV PATH=/opt/psn00bsdk/bin:$PATH

WORKDIR /projeto
CMD ["sh", "-c", "cmake --preset default -DPSN00BSDK_TARGET=mipsel-linux-gnu && cmake --build ./build"]
