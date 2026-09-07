#!/usr/bin/env bash
#
# deploy.sh — baut das Debug-APK, installiert es auf dem per USB angeschlossenen
# Telefon und startet es.
#
#   ./deploy.sh            # bauen, installieren, starten
#   ./deploy.sh --no-build # nur neu installieren und starten
#   ./deploy.sh --run-only # nur neu starten
#
# Die Seite selbst kommt aus ../tools/token-sheet.html und wird von Gradle ins
# APK kopiert. Wer nur die Seite geaendert hat, ruft trotzdem dieses Skript auf:
# ohne Server gibt es keinen anderen Weg aufs Telefon.
#
# JDK und SDK werden gesucht; fehlt etwas, hilft ./setup-toolchain.sh.
# Ueberschreiben geht per JAVA_HOME, ANDROID_SDK_ROOT und ADB.
#
set -euo pipefail

PKG="dev.aaron.tokensheet"
ACTIVITY="$PKG/.MainActivity"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APK="$PROJECT_DIR/app/build/outputs/apk/debug/app-debug.apk"
SHEET="$PROJECT_DIR/../tools/token-sheet.html"

DO_BUILD=1
RUN_ONLY=0
case "${1:-}" in
  --no-build) DO_BUILD=0 ;;
  --run-only) DO_BUILD=0; RUN_ONLY=1 ;;
  -h|--help) sed -n '3,17p' "${BASH_SOURCE[0]}" | sed 's/^# \?//'; exit 0 ;;
  "") ;;
  *) echo "Unbekanntes Argument: $1" >&2; exit 2 ;;
esac

die() { echo "FEHLER: $*" >&2; exit 1; }

# --- Werkzeuge suchen --------------------------------------------------------
find_sdk() {
  for c in "${ANDROID_SDK_ROOT:-}" "${ANDROID_HOME:-}" "$HOME/Android/Sdk"; do
    [ -n "$c" ] && [ -d "$c/platforms" ] && { echo "$c"; return; }
  done
  # Zuletzt das, worauf local.properties zeigt — kann ins Leere laufen, deshalb geprueft.
  if [ -f "$PROJECT_DIR/local.properties" ]; then
    local p; p="$(sed -n 's/^sdk\.dir=//p' "$PROJECT_DIR/local.properties" | tail -1)"
    [ -n "$p" ] && [ -d "$p/platforms" ] && { echo "$p"; return; }
  fi
}

find_jdk() {
  if [ -n "${JAVA_HOME:-}" ] && [ -x "$JAVA_HOME/bin/javac" ]; then echo "$JAVA_HOME"; return; fi
  if command -v javac >/dev/null 2>&1; then
    dirname "$(dirname "$(readlink -f "$(command -v javac)")")"; return
  fi
  # Reihenfolge: das von setup-toolchain.sh ins Home gelegte JDK zuerst.
  for d in "$HOME/Android/jdk-17" /usr/lib/jvm/java-17-openjdk-amd64 /usr/lib/jvm/java-21-openjdk-amd64; do
    [ -x "$d/bin/javac" ] && { echo "$d"; return; }
  done
}

if [ "$RUN_ONLY" -eq 0 ] && [ "$DO_BUILD" -eq 1 ]; then
  JDK="$(find_jdk || true)"
  [ -n "$JDK" ] || die "kein JDK gefunden (nur eine JRE reicht nicht) — hilft: ./setup-toolchain.sh"
  export JAVA_HOME="$JDK"
  SDK="$(find_sdk || true)"
  [ -n "$SDK" ] || die "kein Android SDK gefunden — hilft: ./setup-toolchain.sh"
  export ANDROID_SDK_ROOT="$SDK" ANDROID_HOME="$SDK"
  printf 'sdk.dir=%s\n' "$SDK" > "$PROJECT_DIR/local.properties"
  echo "==> JDK $JAVA_HOME"
  echo "==> SDK $ANDROID_SDK_ROOT"
fi

ADB="${ADB:-$(command -v adb || true)}"
[ -z "$ADB" ] && [ -n "${ANDROID_SDK_ROOT:-}" ] && [ -x "$ANDROID_SDK_ROOT/platform-tools/adb" ] \
  && ADB="$ANDROID_SDK_ROOT/platform-tools/adb"
[ -n "$ADB" ] || die "adb nicht gefunden"

# --- Bauen -------------------------------------------------------------------
if [ "$DO_BUILD" -eq 1 ]; then
  [ -f "$SHEET" ] || die "tools/token-sheet.html fehlt — ohne die Seite hat das APK keinen Inhalt"
  # Damit hinterher klar ist, welche Fassung auf dem Telefon liegt.
  if git -C "$PROJECT_DIR" rev-parse --git-dir >/dev/null 2>&1 \
     && ! git -C "$PROJECT_DIR" diff --quiet -- "$SHEET" 2>/dev/null; then
    echo "==> Hinweis: token-sheet.html hat uneingecheckte Änderungen, die mitgehen"
  fi
  echo "==> Baue das Debug-APK ($(stat -c %s "$SHEET") Byte Seite)"
  (cd "$PROJECT_DIR" && ./gradlew --console=plain :app:assembleDebug)
  [ -f "$APK" ] || die "APK nicht gebaut: $APK"
fi

# --- Telefon -----------------------------------------------------------------
DEVICES="$("$ADB" devices | awk 'NR>1 && $2=="device" {print $1}')"
COUNT="$(printf '%s\n' "$DEVICES" | grep -c . || true)"
[ "$COUNT" -ge 1 ] || die "kein Telefon per adb erreichbar (USB-Debugging an? 'adb devices' prüfen)"
[ "$COUNT" -le 1 ] || die "mehr als ein Gerät verbunden:
$DEVICES
Bitte eines abziehen oder ANDROID_SERIAL setzen."
echo "==> Gerät $DEVICES"

if [ "$RUN_ONLY" -eq 0 ]; then
  [ -f "$APK" ] || die "kein APK vorhanden — einmal ohne --no-build laufen lassen"
  echo "==> Installiere $(basename "$APK") ($(( $(stat -c %s "$APK") / 1024 )) KB)"
  # -r ersetzt, -d erlaubt ein aelteres versionCode beim Herumprobieren.
  "$ADB" install -r -d "$APK"
fi

echo "==> Starte $ACTIVITY"
"$ADB" shell am start -n "$ACTIVITY" >/dev/null

echo
echo "Fertig."
echo "  Logs:   adb logcat -s TokenSheet:* TokenSheetBridge:* TokenSheetShared:* chromium:*"
echo "  Stopp:  adb shell am force-stop $PKG"
