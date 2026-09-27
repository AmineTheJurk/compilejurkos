#!/usr/bin/env bash
set -e

# Find workspace directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$( cd "$SCRIPT_DIR/.." && pwd )"
FIRMWARE_DIR="$PROJECT_DIR/iso/lib/firmware"

echo "Target Firmware Directory: $FIRMWARE_DIR"

mkdir -p "$FIRMWARE_DIR"
mkdir -p "$FIRMWARE_DIR/mediatek"
mkdir -p "$FIRMWARE_DIR/rtw88"
mkdir -p "$FIRMWARE_DIR/rtw89"
mkdir -p "$FIRMWARE_DIR/rtlwifi"
mkdir -p "$FIRMWARE_DIR/ath10k"
mkdir -p "$FIRMWARE_DIR/ath11k"

BASE="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"

INTEL_FILES=(
  "iwlwifi-9560-14.ucode"
  "iwlwifi-9560-34.ucode"
  "iwlwifi-9560-46.ucode"
  "iwlwifi-cc-a0-46.ucode"
  "iwlwifi-cc-a0-59.ucode"
  "iwlwifi-Qu-z0-hr-b0-46.ucode"
  "iwlwifi-QuZ-a0-hr-b0-46.ucode"
  "iwlwifi-ty-a0-gf-a0-59.ucode"
  "iwlwifi-8265-22.ucode"
  "iwlwifi-8265-36.ucode"
  "iwlwifi-8000C-22.ucode"
  "iwlwifi-8000C-36.ucode"
  "iwlwifi-7265D-22.ucode"
  "iwlwifi-7265D-29.ucode"
  "iwlwifi-7265-17.ucode"
  "iwlwifi-7260-17.ucode"
  "iwlwifi-3160-17.ucode"
)

echo "Downloading Intel Wi-Fi firmware..."
for f in "${INTEL_FILES[@]}"; do
  echo "Downloading $f..."
  wget -q -O "$FIRMWARE_DIR/$f" "$BASE/$f" || true
done

MEDIATEK_FILES=(
  "mediatek/mt7601u.bin"
  "mediatek/mt7610e.bin"
  "mediatek/mt7615_firmware.bin"
  "mediatek/mt7663pr2h.bin"
  "mediatek/mt7921_firmware.bin"
  "mediatek/WIFI_RAM_CODE_MT7961_1.bin"
  "mediatek/WIFI_MT7961_patch_mcu_1_1_hdr.bin"
)

echo "Downloading MediaTek Wi-Fi firmware..."
for f in "${MEDIATEK_FILES[@]}"; do
  echo "Downloading $f..."
  wget -q -O "$FIRMWARE_DIR/$f" "$BASE/$f" || true
done

REALTEK_FILES=(
  "rtw88/rtw8822b_fw.bin"
  "rtw88/rtw8822c_fw.bin"
  "rtw88/rtw8821c_fw.bin"
  "rtw88/rtw8723d_fw.bin"
  "rtw89/rtw8852a_fw.bin"
  "rtw89/rtw8852c_fw.bin"
  "rtlwifi/rtl8723befw.bin"
  "rtlwifi/rtl8821aefw.bin"
  "rtlwifi/rtl8192cfw.bin"
  "rtlwifi/rtl8192eefw.bin"
)

echo "Downloading Realtek Wi-Fi firmware..."
for f in "${REALTEK_FILES[@]}"; do
  echo "Downloading $f..."
  wget -q -O "$FIRMWARE_DIR/$f" "$BASE/$f" || true
done

OTHER_FILES=(
  "rt2870.bin"
  "rt2800.bin"
  "htc_9271.fw"
  "htc_7010.fw"
)

echo "Downloading Atheros / Ralink firmware..."
for f in "${OTHER_FILES[@]}"; do
  echo "Downloading $f..."
  wget -q -O "$FIRMWARE_DIR/$f" "$BASE/$f" || true
done

echo "Firmware download complete!"
