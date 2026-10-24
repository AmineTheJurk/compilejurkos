#!/usr/bin/env bash
# Copyright (c) 2026 AmineTheJurk
# JurkOS Kernel Compilation Script
# Compiles both 64-bit and 32-bit kernels

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================================="
echo "          JurkOS Kernel Build Toolchain"
echo "=========================================================="
echo "[*] Working directory: $SCRIPT_DIR"

# Check for required tools
MISSING_TOOLS=()
for tool in gcc make ld; do
    if ! command -v "$tool" &>/dev/null; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    echo "[!] Missing required build tools: ${MISSING_TOOLS[*]}"
    echo "[*] Attempting automatic installation via apt..."
    if command -v apt-get &>/dev/null; then
        if [ "$EUID" -eq 0 ]; then
            apt-get update && apt-get install -y build-essential gcc-multilib make binutils
        elif command -v sudo &>/dev/null; then
            sudo apt-get update && sudo apt-get install -y build-essential gcc-multilib make binutils
        else
            echo "[-] Please run: apt-get update && apt-get install -y build-essential gcc-multilib make binutils"
            exit 1
        fi
    else
        echo "[-] Package manager not recognized. Please install GCC, Make, and Binutils."
        exit 1
    fi
fi

echo "[*] Cleaning previous build artifacts..."
make clean 2>/dev/null || true

echo ""
echo "----------------------------------------------------------"
echo " [1/2] Compiling 64-bit Kernel (x86_64)..."
echo "----------------------------------------------------------"
make kernel-x86_64.elf

echo ""
echo "----------------------------------------------------------"
echo " [2/2] Compiling 32-bit Kernel (x86)..."
echo "----------------------------------------------------------"
make kernel-x86.elf

echo ""
echo "----------------------------------------------------------"
echo " [*] Deploying kernel artifacts to iso/boot/limine/..."
echo "----------------------------------------------------------"

ISO_DIR="$SCRIPT_DIR/../../iso/boot/limine"
if [ ! -d "$ISO_DIR" ]; then
    ISO_DIR="$SCRIPT_DIR/../iso/boot/limine"
fi
mkdir -p "$ISO_DIR"

cp -fv kernel-x86_64.elf "$ISO_DIR/kernel-x86_64"
cp -fv kernel-x86.elf "$ISO_DIR/kernel-x86"

echo ""
echo "=========================================================="
echo " [SUCCESS] JurkOS Kernels Compiled Successfully!"
echo "=========================================================="
ls -lh "$ISO_DIR/kernel"*
echo "=========================================================="
