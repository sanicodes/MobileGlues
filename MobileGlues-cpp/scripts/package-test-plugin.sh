#!/usr/bin/env bash
# Compatibility entry point for the earlier test APK packaging instructions.
set -euo pipefail
MOBILEGLUES_CHANNEL=test exec bash "$(dirname "$0")/package-plugin.sh" "$@"
