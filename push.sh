#!/usr/bin/env bash
# Copyright (c) 2026 AmineTheJurk
# JurkOS Automated Git Push Script (runs inside Git Bash or POSIX shell)

set -e

echo "========================================================"
echo "      JurkOS Automated Git Push Script (Git Bash)"
echo "========================================================"
echo ""

# Ensure we are in the script's directory
cd "$(dirname "$0")"

echo "[*] Initializing Git repository if needed..."
git init

echo "[*] Adding all files to staging..."
git add -A

echo "[*] Setting remote origin..."
git remote remove origin 2>/dev/null || true
git remote add origin https://github.com/AmineTheJurk/compilejurkos/

read -r -p "Enter commit message (default: 'Update JurkOS repository'): " commitname
if [ -z "$commitname" ]; then
    commitname="Update JurkOS repository"
fi

echo "[*] Committing changes with message: '$commitname'..."
git commit -m "$commitname" || true

echo "[*] Renaming branch to main..."
git branch -M main

echo "[*] Force pushing to origin main..."
git push -u origin main --force

echo ""
echo "========================================================"
echo " [SUCCESS] Pushed to https://github.com/AmineTheJurk/compilejurkos/"
echo "========================================================"
