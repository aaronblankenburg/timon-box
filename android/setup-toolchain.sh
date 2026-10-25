#!/usr/bin/env bash
#
# setup-toolchain.sh — installiert einmalig, was zum Bauen der App fehlt: ein JDK
# und das Android SDK. Danach genuegt ./deploy.sh.
#
#   ./setup-toolchain.sh            # JDK per apt, SDK nach ~/Android/Sdk
#   ./setup-toolchain.sh --check    # nur pruefen, nichts installieren
#   SDK_ROOT=/anderer/pfad ./setup-toolchain.sh
#
# Das SDK landet bewusst in einem festen Pfad im Home. Die beiden aelteren
# Android-Projekte hier zeigen per sdk.dir in Scratchpad-Verzeichnisse, die es
# nicht mehr gibt — genau das soll hier nicht wieder passieren.
#
# Achtung: das Skript akzeptiert die Android-SDK-Lizenzen in deinem Namen.
#
set -euo pipefail

SDK_ROOT="${SDK_ROOT:-$HOME/Android/Sdk}"
JDK_HOME="${JDK_HOME:-$HOME/Android/jdk-17}"
JDK_URL="${JDK_URL:-https://api.adoptium.net/v3/binary/latest/17/ga/linux/x64/jdk/hotspot/normal/eclipse}"
CMDLINE_ZIP="${CMDLINE_ZIP:-commandlinetools-linux-11076708_latest.zip}"
PLATFORM="${PLATFORM:-android-35}"
BUILD_TOOLS="${BUILD_TOOLS:-35.0.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CHECK_ONLY=0
case "${1:-}" in
  --check) CHECK_ONLY=1 ;;
  -h|--help) sed -n '3,15p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
  "") ;;
  *) echo "Unbekanntes Argument: $1" >&2; exit 2 ;;
esac

die() { echo "FEHLER: $*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# --- Bestandsaufnahme --------------------------------------------------------
echo "==> Bestandsaufnahme"
JDK_OK=0
# Ein frueher hierher gelegtes JDK steht nicht im PATH — trotzdem benutzbar.
if [ -x "$JDK_HOME/bin/javac" ]; then
  export JAVA_HOME="$JDK_HOME"
  export PATH="$JDK_HOME/bin:$PATH"
fi
if have javac; then
  JDK_MAJOR="$(javac -version 2>&1 | sed -n 's/^javac \([0-9]\+\).*/\1/p')"
  [ -n "$JDK_MAJOR" ] && [ "$JDK_MAJOR" -ge 17 ] && JDK_OK=1
  echo "    javac $(javac -version 2>&1 | cut -d' ' -f2)"
else
  echo "    javac fehlt (nur eine JRE installiert reicht nicht)"
fi

SDK_OK=0
if [ -d "$SDK_ROOT/platforms/$PLATFORM" ] && [ -d "$SDK_ROOT/build-tools/$BUILD_TOOLS" ]; then
  SDK_OK=1
  echo "    SDK vollstaendig in $SDK_ROOT"
else
  echo "    SDK unvollstaendig in $SDK_ROOT ($PLATFORM / build-tools $BUILD_TOOLS)"
fi

have adb && echo "    adb $(adb version | head -1 | awk '{print $NF}')" || echo "    adb fehlt"

if [ "$CHECK_ONLY" -eq 1 ]; then
  [ "$JDK_OK" -eq 1 ] && [ "$SDK_OK" -eq 1 ] && { echo; echo "Alles da."; exit 0; }
  echo; echo "Es fehlt etwas. Ohne --check installiert das Skript es."
  exit 1
fi

# --- JDK ---------------------------------------------------------------------
# AGP 8.7 verlangt mindestens JDK 17. Auf dieser Maschine liegen nur JREs.
#
# Zwei Wege: apt, wenn sudo ohne Rueckfrage durchgeht, sonst ein Temurin-Tarball
# ins Home. Der zweite Weg braucht kein root und laesst das System in Ruhe —
# wichtig, weil dieses Skript auch unbeaufsichtigt laufen koennen soll.
if [ "$JDK_OK" -eq 0 ]; then
  if have apt-get && sudo -n true 2>/dev/null; then
    echo "==> Installiere openjdk-17-jdk per apt"
    sudo -n apt-get install -y openjdk-17-jdk unzip
  else
    echo "==> Kein passwortloses sudo — hole ein Temurin-JDK nach $JDK_HOME"
    have curl || die "curl fehlt"
    have tar || die "tar fehlt"
    mkdir -p "$(dirname "$JDK_HOME")"
    TMPJ="$(mktemp -d)"
    curl -fL --progress-bar -o "$TMPJ/jdk.tar.gz" "$JDK_URL" \
      || { rm -rf "$TMPJ"; die "JDK-Download fehlgeschlagen: $JDK_URL"; }
    tar -xzf "$TMPJ/jdk.tar.gz" -C "$TMPJ"
    UNPACKED="$(find "$TMPJ" -maxdepth 1 -type d -name 'jdk-*' | head -1)"
    [ -n "$UNPACKED" ] || { rm -rf "$TMPJ"; die "Archiv enthaelt kein jdk-Verzeichnis"; }
    rm -rf "$JDK_HOME"
    mv "$UNPACKED" "$JDK_HOME"
    rm -rf "$TMPJ"
    export JAVA_HOME="$JDK_HOME"
    export PATH="$JDK_HOME/bin:$PATH"
    echo "    JDK liegt in $JDK_HOME (nur fuer dieses Projekt, das System bleibt unberuehrt)"
  fi
  have javac || die "javac fehlt weiterhin"
  echo "    $(javac -version 2>&1)"
fi

# --- Android SDK -------------------------------------------------------------
if [ "$SDK_OK" -eq 0 ]; then
  SDKMANAGER="$SDK_ROOT/cmdline-tools/latest/bin/sdkmanager"
  if [ ! -x "$SDKMANAGER" ]; then
    echo "==> Lade die Command-line Tools nach $SDK_ROOT"
    have curl || die "curl fehlt"
    have unzip || die "unzip fehlt — sudo apt install unzip"
    mkdir -p "$SDK_ROOT/cmdline-tools"
    TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
    curl -fL --progress-bar -o "$TMP/tools.zip" \
      "https://dl.google.com/android/repository/$CMDLINE_ZIP" \
      || die "Download fehlgeschlagen: $CMDLINE_ZIP"
    unzip -q "$TMP/tools.zip" -d "$TMP"
    # Im Archiv heisst das Verzeichnis cmdline-tools; sdkmanager erwartet es
    # unter cmdline-tools/latest, sonst findet er sein eigenes SDK nicht.
    rm -rf "$SDK_ROOT/cmdline-tools/latest"
    mv "$TMP/cmdline-tools" "$SDK_ROOT/cmdline-tools/latest"
    [ -x "$SDKMANAGER" ] || die "sdkmanager nicht an erwarteter Stelle: $SDKMANAGER"
  fi

  echo "==> Akzeptiere die Android-SDK-Lizenzen in deinem Namen"
  yes | "$SDKMANAGER" --sdk_root="$SDK_ROOT" --licenses >/dev/null || true

  echo "==> Installiere platform-tools, platforms;$PLATFORM, build-tools;$BUILD_TOOLS"
  "$SDKMANAGER" --sdk_root="$SDK_ROOT" \
    "platform-tools" "platforms;$PLATFORM" "build-tools;$BUILD_TOOLS" >/dev/null

  [ -d "$SDK_ROOT/platforms/$PLATFORM" ] || die "$PLATFORM wurde nicht installiert"
  [ -d "$SDK_ROOT/build-tools/$BUILD_TOOLS" ] || die "build-tools $BUILD_TOOLS wurden nicht installiert"
fi

# --- local.properties --------------------------------------------------------
printf 'sdk.dir=%s\n' "$SDK_ROOT" > "$SCRIPT_DIR/local.properties"
echo "==> $SCRIPT_DIR/local.properties zeigt auf $SDK_ROOT"

echo
if [ -x "$JDK_HOME/bin/javac" ]; then
  echo "Hinweis: das JDK liegt in $JDK_HOME, nicht im System. deploy.sh findet es dort."
fi
echo "Fertig. Weiter mit:  ./deploy.sh"
