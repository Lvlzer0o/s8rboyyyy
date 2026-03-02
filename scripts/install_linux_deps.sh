#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  SUDO="sudo"
else
  SUDO=""
fi

export DEBIAN_FRONTEND=noninteractive

${SUDO} apt-get update
${SUDO} apt-get install -y --no-install-recommends \
  build-essential \
  cmake \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libsdl2-dev \
  libsdl2-ttf-dev

echo "Installed build/runtime dependencies for OneShot Skate."
