#!/usr/bin/env bash
# Деплой прошивки VladiksLED по OTA: сборка → отчёт о размере → заливка espota → проверка, что устройство поднялось.
#
# Использование:
#   .claude/skills/deploy/deploy.sh            # сборка + OTA + проверка
#   .claude/skills/deploy/deploy.sh --build    # только сборка и размеры, без заливки
#   .claude/skills/deploy/deploy.sh --verify   # только проверка устройства (/api/debug)
#
# Логи пишутся в temp/deploy-*.log (temp/ в .gitignore).
# ВАЖНО для агентов: OTA (UDP 8266 + входящий TCP) не проходит в песочнице Bash —
# запускать с отключённой песочницей, иначе espota падает с "No Answer".

set -euo pipefail

# Корень репозитория — два уровня вверх от папки скилла
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"

ENV_NAME="d1_mini"
mkdir -p temp

# PlatformIO: берём из PATH, иначе из pipx-установки
PIO="$(command -v pio || true)"
if [[ -z "$PIO" && -x "$HOME/.local/bin/pio" ]]; then PIO="$HOME/.local/bin/pio"; fi
if [[ -z "$PIO" ]]; then
  echo "❌ PlatformIO не найден. Установить: pipx install platformio && pio pkg install -e $ENV_NAME" >&2
  exit 2
fi

# IP устройства берём из upload_port в platformio.ini, чтобы не дублировать настройку
DEVICE_IP="$(sed -nE 's/^[[:space:]]*upload_port[[:space:]]*=[[:space:]]*([0-9.]+).*/\1/p' platformio.ini | head -1)"
if [[ -z "$DEVICE_IP" ]]; then
  echo "❌ Не найден upload_port в platformio.ini" >&2
  exit 2
fi

MODE="${1:-all}"

# Сборка прошивки и вывод строк RAM/Flash
build() {
  echo "🔨 Сборка ($ENV_NAME)..."
  if ! "$PIO" run -e "$ENV_NAME" > temp/deploy-build.log 2>&1; then
    grep -nE "error|Error" temp/deploy-build.log | head -20 || true
    echo "❌ Сборка упала, полный лог: temp/deploy-build.log" >&2
    exit 1
  fi
  grep -E "^(RAM|Flash):" temp/deploy-build.log || true
  local warns
  warns="$(grep -c "warning:" temp/deploy-build.log || true)"
  echo "⚠️  Предупреждений компилятора: ${warns:-0}"
}

# Текущий uptime устройства в мс (пусто, если не отвечает)
device_uptime() {
  curl -s -m 3 "http://$DEVICE_IP/api/debug" | sed -nE 's/.*"uptimeMs":([0-9]+).*/\1/p'
}

# Заливка по OTA; перед этим убеждаемся, что устройство в сети.
# Шлём gzip-образ напрямую через espota.py: ядро ESP8266 3.x само распаковывает
# сжатую прошивку (≈−35% трафика), а прямой вызов не перезапускает сборку,
# как это делает `pio run -t upload`.
UPTIME_BEFORE=""
upload() {
  echo "📡 Проверка доступности $DEVICE_IP..."
  UPTIME_BEFORE="$(device_uptime || true)"
  if [[ -z "$UPTIME_BEFORE" ]]; then
    echo "❌ Устройство $DEVICE_IP не отвечает по HTTP — OTA невозможна" >&2
    exit 1
  fi
  local bin=".pio/build/$ENV_NAME/firmware.bin"
  local espota="$HOME/.platformio/packages/framework-arduinoespressif8266/tools/espota.py"
  gzip -9 -c "$bin" > "$bin.gz"
  echo "⬆️  OTA-заливка ($(( $(wc -c < "$bin.gz") / 1024 )) КБ gzip из $(( $(wc -c < "$bin") / 1024 )) КБ)..."
  if ! python3 "$espota" -i "$DEVICE_IP" -p 8266 -f "$bin.gz" -r > temp/deploy-upload.log 2>&1; then
    grep -E "ERROR|Error" temp/deploy-upload.log | tail -5 || true
    echo "❌ OTA не удалась, лог: temp/deploy-upload.log" >&2
    echo "   'No Answer' при живом HTTP почти всегда = песочница/файрвол блокирует UDP 8266." >&2
    exit 1
  fi
  echo "✅ Образ принят устройством"
}

# Ждём перезагрузки и печатаем ключевые поля /api/debug.
# После upload ждём, пока uptime станет меньше, чем был до заливки: espota
# возвращает OK раньше, чем плата уходит в ребут, и старая прошивка ещё отвечает.
verify() {
  echo "🔎 Ожидание устройства после перезагрузки..."
  local resp="" up=""
  for _ in $(seq 1 40); do
    resp="$(curl -s -m 2 "http://$DEVICE_IP/api/debug" || true)"
    up="$(echo "$resp" | sed -nE 's/.*"uptimeMs":([0-9]+).*/\1/p')"
    if [[ -n "$up" && ( -z "$UPTIME_BEFORE" || "$up" -lt "$UPTIME_BEFORE" ) ]]; then break; fi
    up=""
    sleep 1.5
  done
  if [[ -z "$up" ]]; then
    echo "❌ Устройство не поднялось с новой прошивкой за ~60 с" >&2
    exit 1
  fi
  # Компактный вывод полей без зависимостей (jq может отсутствовать)
  echo "$resp" | grep -oE '"(uptimeMs|timeValid|formattedTime|wifiRSSI|freeHeap|maxFreeBlock)":[^,}]*' | tr '\n' ' '
  echo
  [[ "$resp" == *'"timeValid":true'* ]] || echo "⚠️  timeValid=false: время ещё не синхронизировано (подождать или проверить NTP/DNS)"
  echo "✅ Устройство отвечает"
}

case "$MODE" in
  --build)  build ;;
  --verify) verify ;;
  all)      build; upload; verify ;;
  *) echo "Неизвестный аргумент: $MODE (допустимо: --build, --verify)" >&2; exit 2 ;;
esac
