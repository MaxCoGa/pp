# Multi-stage build for static pp binary using musl-libc
# Stage 1: Build environment
FROM alpine:latest AS builder

# Install build dependencies
RUN apk add --no-cache \
    gcc \
    musl-dev \
    make \
        file \
    curl-dev \
    curl-static \
    openssl-libs-static \
    nghttp2-static \
        nghttp3-static \
    brotli-static \
    zlib-static \
    zstd-static \
    libidn2-static \
    libpsl-static \
    libunistring-static \
    c-ares-dev

# Set working directory
WORKDIR /build

# Copy source code
COPY pp.c .

# Compile statically
RUN gcc -o pp pp.c \
    -static \
    -lcurl \
    -lnghttp2 \
        -lnghttp3 \
    -lssl \
    -lcrypto \
        -lzstd \
        -lidn2 \
        -lpsl \
        -lunistring \
        -lcares \
    -lz \
    -lbrotlidec \
    -lbrotlicommon \
    -lpthread \
    && strip pp

# Verify it's statically linked
RUN file pp && \
    (ldd pp 2>&1 | grep -qE "not a dynamic executable|statically linked" || file pp | grep -q "static-pie") && \
    echo "Binary is statically linked!"

# Stage 2: Minimal runtime (optional - for testing)
FROM scratch AS runtime
COPY --from=builder /build/pp /pp
ENTRYPOINT ["/pp"]

# Stage 3: Export binary only
FROM alpine:latest AS exporter
COPY --from=builder /build/pp /pp
CMD ["cp", "/pp", "/output/pp"]
