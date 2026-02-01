#!/bin/bash
# =============================================================================
# LombardEar Mac Audio Setup Script
# =============================================================================
# Sets up macOS Aggregate Device for Hollyland Lark A1 (2x Rx2 dongles)
# =============================================================================

set -e

echo "=============================================="
echo "LombardEar Audio Setup for Lark A1"
echo "=============================================="
echo ""
echo "This script guides you through setting up an Aggregate Device"
echo "for 4-channel audio from two Lark A1 Rx2 dongles."
echo ""

# Check if Audio MIDI Setup is accessible
if [ ! -d "/Applications/Utilities/Audio MIDI Setup.app" ]; then
    echo "WARNING: Audio MIDI Setup not found at expected location."
fi

echo "=== STEP 1: Connect Hardware ==="
echo "1. Plug in both Lark A1 Rx2 USB-C dongles"
echo "2. Pair transmitters to receivers"
echo ""
read -p "Press Enter when ready..."

echo ""
echo "=== STEP 2: Create Aggregate Device ==="
echo "1. Open Audio MIDI Setup (Cmd+Space, type 'Audio MIDI Setup')"
echo "2. Click the '+' button at bottom left"
echo "3. Select 'Create Aggregate Device'"
echo "4. Check BOTH Lark A1 devices in the list"
echo "5. Rename it to 'LombardEar 4ch'"
echo ""
echo "IMPORTANT: Note the order of devices - this determines channel mapping!"
echo "  - First device  = Channels 1-2 (Temple Left, Temple Right)"
echo "  - Second device = Channels 3-4 (Back Left, Back Right)"
echo ""
read -p "Press Enter when Aggregate Device is created..."

# List available audio devices
echo ""
echo "=== STEP 3: Verify Devices ==="
echo "Available audio devices:"
echo ""

# Use system_profiler to list audio devices
system_profiler SPAudioDataType 2>/dev/null | grep -A2 "Input Source" || true

echo ""
echo "=== STEP 4: Test with LombardEar ==="
echo ""
echo "Run LombardEar to test the setup:"
echo "  cd $(dirname "$0")/../build"
echo "  ./lombardear"
echo ""
echo "In the WebSocket UI (http://localhost:8000), verify:"
echo "  - All 4 channels show signal (l, r, b, e meters)"
echo "  - Jitter stats are updating"
echo "  - Phase offsets are reasonable (<10 samples)"
echo ""
echo "=============================================="
echo "Setup Complete!"
echo "=============================================="
