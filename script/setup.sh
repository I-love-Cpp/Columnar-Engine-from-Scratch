#!/usr/bin/env bash
set -euo pipefail

echo "=== Installing dependencies ==="

apt-get update
apt-get install -y --no-install-recommends \
    cmake \
    g++ \
    make \
    ninja-build \
    ca-certificates

rm -rf /var/lib/apt/lists/*

echo "=== Done ==="
