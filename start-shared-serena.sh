#!/usr/bin/env bash

# Общий лаунчер Serena для нескольких одновременных сессий Codex и Claude Code.
#
# Зачем нужен этот файл
# ---------------------
# Клиент умеет запускать MCP-сервер через stdio, но прямой запуск Serena в таком
# режиме создавал бы отдельный тяжёлый процесс Serena для каждой сессии. Этот
# скрипт разделяет роли:
#
# 1. Один общий процесс Serena работает как Streamable HTTP MCP на localhost.
# 2. Каждая сессия клиента запускает только лёгкий stdio-мост `mcp-remote`.
# 3. Все мосты одного порта подключаются к одному HTTP-процессу Serena.
# 4. Через 30 минут после закрытия последнего моста общая Serena останавливается.
# 5. Новая сессия снова автоматически поднимает Serena при необходимости.
#
# Таким образом, модель работы выглядит как «N сессий → 1 Serena на порт». Для
# разных проектов необходимо назначать разные TCP-порты: порт является системным
# ресурсом и не может одновременно принадлежать двум Serena.
#
# Контекст Serena и деление инстанса
# ----------------------------------
# Контекст (`--context`) задаётся процессу Serena при старте и распространяется
# на все подключения к нему: одно соединение не может получить контекст, отличный
# от соседнего. Поэтому клиентам, которым нужны разные контексты, нужны разные
# порты, и модель здесь такая: один инстанс на клиента, общий для всех его сессий.
#
#   порт 9121, --context codex        → все сессии Codex
#   порт 9123, --context claude-code  → все сессии Claude Code
#
# Контекст намеренно не входит в ключ runtime-каталога: инстанс идентифицируется
# проектом, host и портом. Если на порту уже работает Serena с другим контекстом,
# скрипт не перезапускает её, а печатает предупреждение в stderr.
#
# Как подключить в проекте
# ------------------------
# В локальном `.codex/config.toml` проекта нужно зарегистрировать stdio-сервер:
#
#   [mcp_servers.my_project_serena]
#   enabled = true
#   command = "/абсолютный/путь/к/start-shared-serena.sh"
#   args = [
#     "--stdio-bridge",
#     "--project", "/абсолютный/путь/к/проекту",
#     "--port", "9121",
#     "--context", "codex",
#     "--idle-timeout", "1800",
#   ]
#   startup_timeout_sec = 40
#   tool_timeout_sec = 120
#
# Для Claude Code то же самое живёт в `.mcp.json` проекта:
#
#   "serena": {
#     "type": "stdio",
#     "command": "/абсолютный/путь/к/start-shared-serena.sh",
#     "args": [
#       "--stdio-bridge",
#       "--project", "/абсолютный/путь/к/проекту",
#       "--port", "9123",
#       "--context", "claude-code",
#       "--idle-timeout", "1800"
#     ]
#   }
#
# После изменения конфигурации клиент необходимо перезапустить один раз, потому
# что список MCP-серверов загружается при создании сессии. Отдельный
# SessionStart-хук для Serena не требуется.
#
# Режимы запуска
# --------------
# - Без параметров: foreground-режим для ручной диагностики; Ctrl+C завершает
#   принадлежащий терминалу процесс Serena.
# - `--background`: идемпотентно запускает общую Serena в фоне либо сообщает,
#   что подходящий процесс уже работает.
# - `--stdio-bridge`: режим для клиента; регистрирует активную сессию, гарантирует
#   запуск общей Serena и проксирует stdio в её HTTP endpoint.
# - `--idle-timeout 1800`: время простоя в секундах; значение `0` отключает
#   автоматическую остановку.
#
# Как обеспечивается единственный инстанс
# ---------------------------------------
# Runtime-каталог вычисляется по хешу абсолютного пути проекта, host и port.
# В нём хранятся PID, лог, атомарная lock-ссылка и leases активных stdio-мостов.
# Проверяется не только PID, но и команда процесса с ожидаемыми проектом и
# портом, поэтому другой проект не будет ошибочно принят за нужную Serena.
# Параллельные старты сериализуются lock-файлом.
#
# Фоновый процесс на macOS запускается временной задачей `launchctl submit`.
# Она не устанавливает plist и не добавляет Serena в автозапуск при входе, но
# отделяет её от конкретной сессии Codex. На других системах используется
# fallback через `nohup`.
#
# Контроль простоя
# ----------------
# Каждый stdio-мост создаёт lease с PID своего процесса. Watchdog удаляет
# протухшие leases, а после заданного периода без живых мостов останавливает
# только Serena с совпадающими проектом и портом. Перед остановкой он повторно
# берёт launch-lock и проверяет leases, поэтому новая сессия не гоняется с
# таймаутом: либо отменит остановку, либо сразу поднимет новый процесс.
# После остановки Serena остаётся только лёгкий спящий watchdog; он нужен, чтобы
# обслуживать следующие циклы запуска, и удаляется macOS при выходе пользователя.
#
# Зависимости: `serena`, `curl`, `lsof`, `npx` и закреплённый ниже пакет
# `mcp-remote`. Логи находятся в `/tmp/shared-serena/<hash>/`.

set -Eeuo pipefail

readonly SERENA_BIN="${SERENA_BIN:-serena}"
readonly NPX_BIN="${NPX_BIN:-npx}"
readonly MCP_REMOTE_PACKAGE="${MCP_REMOTE_PACKAGE:-mcp-remote@0.8.6}"
readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly SCRIPT_PATH="${SCRIPT_DIR}/$(basename -- "${BASH_SOURCE[0]}")"
readonly LAUNCH_ENV_PATH="${PATH:-/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin}"
readonly LOG_NOISE_PATTERN="Unhandled method '\$/analyzerStatus'"

SERENA_HOST="${SERENA_MCP_HOST:-127.0.0.1}"
SERENA_PORT="${SERENA_MCP_PORT:-9121}"
# Скрипт лежит в корне репозитория, поэтому проект по умолчанию — каталог скрипта.
PROJECT_DIR="${SERENA_PROJECT_DIR:-${SCRIPT_DIR}}"
# Контекст процесса Serena; `codex` оставлен по умолчанию ради обратной совместимости
# с уже настроенными вызовами лаунчера без явного `--context`.
SERENA_CONTEXT="${SERENA_MCP_CONTEXT:-codex}"
IDLE_TIMEOUT_SECONDS="${SERENA_IDLE_TIMEOUT_SECONDS:-1800}"
LAUNCHER_SCRIPT_PATH="${SCRIPT_PATH}"
BACKGROUND_MODE=false
STDIO_BRIDGE_MODE=false
IDLE_WATCHDOG_MODE=false
SERENA_PID=""
BRIDGE_PROXY_PID=""
BRIDGE_LEASE_FILE=""
RUNTIME_DIR=""
PID_FILE=""
LOG_FILE=""
LOCK_FILE=""
LEASE_DIR=""
WATCHDOG_PID_FILE=""
WATCHDOG_SCRIPT_PATH=""
LAUNCHD_LABEL=""
WATCHDOG_LAUNCHD_LABEL=""
SERENA_URL=""
SERENA_ARGUMENTS=()
SERENA_EXECUTABLE=""

# Показывает поддерживаемые параметры запуска.
print_usage() {
  cat <<'EOF'
Использование:
  ./start-shared-serena.sh [--background | --stdio-bridge] [--project ПУТЬ] [--port ПОРТ] [--host ХОСТ] [--context ИМЯ] [--idle-timeout СЕКУНДЫ]

Параметры:
  -b, --background   Запустить Serena независимо от терминала. Повторный вызов безопасен.
  --stdio-bridge     Поднять общую Serena и проксировать stdio клиента в её HTTP endpoint.
  -p, --project      Абсолютный или относительный путь к проекту Serena (по умолчанию корень репозитория).
  -P, --port         TCP-порт Streamable HTTP MCP.
  -c, --context      Контекст Serena: codex, claude-code и т.п. (по умолчанию codex).
                     Принадлежит процессу, а не соединению, поэтому клиентам с разными
                     контекстами нужны разные порты.
  -H, --host         Адрес прослушивания (по умолчанию 127.0.0.1).
  --idle-timeout     Остановить Serena после простоя; 0 отключает таймаут (по умолчанию 1800).
  -h, --help         Показать эту справку.

Без --background Serena работает в foreground до Ctrl+C.
EOF
}

# Разбирает именованные параметры командной строки.
parse_arguments() {
  while (($# > 0)); do
    case "$1" in
      -b | --background | background)
        BACKGROUND_MODE=true
        shift
        ;;
      --stdio-bridge)
        BACKGROUND_MODE=true
        STDIO_BRIDGE_MODE=true
        shift
        ;;
      --idle-watchdog)
        BACKGROUND_MODE=true
        IDLE_WATCHDOG_MODE=true
        shift
        ;;
      --launcher-script)
        [[ $# -ge 2 ]] || {
          printf 'Ошибка: после %s требуется путь к исходному лаунчеру.\n' "$1" >&2
          return 1
        }
        LAUNCHER_SCRIPT_PATH="$2"
        shift 2
        ;;
      -p | --project)
        [[ $# -ge 2 ]] || {
          printf 'Ошибка: после %s требуется путь к проекту.\n' "$1" >&2
          return 1
        }
        PROJECT_DIR="$2"
        shift 2
        ;;
      -P | --port)
        [[ $# -ge 2 ]] || {
          printf 'Ошибка: после %s требуется порт.\n' "$1" >&2
          return 1
        }
        SERENA_PORT="$2"
        shift 2
        ;;
      -H | --host)
        [[ $# -ge 2 ]] || {
          printf 'Ошибка: после %s требуется адрес.\n' "$1" >&2
          return 1
        }
        SERENA_HOST="$2"
        shift 2
        ;;
      -c | --context)
        [[ $# -ge 2 ]] || {
          printf 'Ошибка: после %s требуется имя контекста.\n' "$1" >&2
          return 1
        }
        SERENA_CONTEXT="$2"
        shift 2
        ;;
      --idle-timeout)
        [[ $# -ge 2 ]] || {
          printf 'Ошибка: после %s требуется число секунд.\n' "$1" >&2
          return 1
        }
        IDLE_TIMEOUT_SECONDS="$2"
        shift 2
        ;;
      -h | --help)
        print_usage
        exit 0
        ;;
      *)
        printf 'Ошибка: неизвестный параметр: %s\n' "$1" >&2
        print_usage >&2
        return 1
        ;;
    esac
  done
}

# Проверяет параметры и вычисляет отдельный runtime-каталог для проекта и порта.
initialize_runtime() {
  local runtime_checksum
  local runtime_key

  [[ "${SERENA_PORT}" =~ ^[0-9]+$ ]] && ((SERENA_PORT >= 1 && SERENA_PORT <= 65535)) || {
    printf 'Ошибка: порт должен быть целым числом от 1 до 65535: %s\n' "${SERENA_PORT}" >&2
    return 1
  }

  [[ "${IDLE_TIMEOUT_SECONDS}" =~ ^[0-9]+$ ]] || {
    printf 'Ошибка: idle timeout должен быть целым числом от 0: %s\n' "${IDLE_TIMEOUT_SECONDS}" >&2
    return 1
  }

  [[ "${SERENA_CONTEXT}" =~ ^[A-Za-z0-9._-]+$ ]] || {
    printf 'Ошибка: имя контекста должно состоять из букв, цифр, точки, дефиса или подчёркивания: %s\n' "${SERENA_CONTEXT}" >&2
    return 1
  }

  [[ -d "${PROJECT_DIR}" ]] || {
    printf 'Ошибка: директория проекта не найдена: %s\n' "${PROJECT_DIR}" >&2
    return 1
  }

  PROJECT_DIR="$(cd -- "${PROJECT_DIR}" && pwd -P)"
  SERENA_URL="http://${SERENA_HOST}:${SERENA_PORT}/mcp"

  # Хеш не даёт разным проектам делить PID-файл и лог. Контекст в ключ не входит
  # намеренно: инстанс адресуется портом, и добавление контекста развело бы leases
  # уже работающих мостов с новым watchdog'ом.
  runtime_checksum="$(printf '%s' "${PROJECT_DIR}|${SERENA_HOST}|${SERENA_PORT}" | cksum)"
  runtime_key="${runtime_checksum%% *}"
  RUNTIME_DIR="/tmp/shared-serena/${runtime_key}"
  PID_FILE="${RUNTIME_DIR}/serena.pid"
  LOG_FILE="${RUNTIME_DIR}/serena.log"
  LOCK_FILE="${RUNTIME_DIR}/launch.lock"
  LEASE_DIR="${RUNTIME_DIR}/leases"
  WATCHDOG_PID_FILE="${RUNTIME_DIR}/watchdog.pid"
  WATCHDOG_SCRIPT_PATH="${RUNTIME_DIR}/watchdog-launcher.sh"
  LAUNCHD_LABEL="local.shared-serena.${runtime_key}"
  WATCHDOG_LAUNCHD_LABEL="${LAUNCHD_LABEL}.watchdog"
}

# Выводит готовую конфигурацию HTTP MCP-сервера для Codex.
print_codex_config() {
  printf '\nSerena доступна по адресу: %s\n' "${SERENA_URL}"
  printf 'Проект Serena: %s\n' "${PROJECT_DIR}"
  printf 'Контекст Serena: %s\n\n' "${SERENA_CONTEXT}"
  printf '%s\n' '[mcp_servers.serena]'
  printf 'url = "%s"\n' "${SERENA_URL}"
  printf '%s\n' 'startup_timeout_sec = 40'
  printf '%s\n' 'tool_timeout_sec = 120'
}

# Проверяет сигнатуру endpoint Streamable HTTP сервера Serena.
is_serena_endpoint_ready() {
  local http_code

  http_code="$(curl --head --silent --output /dev/null --write-out '%{http_code}' --max-time 1 "${SERENA_URL}" || true)"
  [[ "${http_code}" == "405" ]]
}

# Проверяет, что PID принадлежит Serena с ожидаемыми проектом и портом.
is_expected_serena_process() {
  local process_pid="$1"
  local process_command

  [[ "${process_pid}" =~ ^[0-9]+$ ]] || return 1
  kill -0 "${process_pid}" 2>/dev/null || return 1
  process_command="$(ps -p "${process_pid}" -o command= 2>/dev/null || true)"

  [[ "${process_command}" == *"start-mcp-server"* ]]
  [[ "${process_command}" == *"--port ${SERENA_PORT}"* ]]
  [[ "${process_command}" == *"--project ${PROJECT_DIR}"* ]]
}

# Возвращает успех, когда PID-файл указывает на ожидаемый процесс Serena.
is_recorded_serena_running() {
  local recorded_pid

  [[ -f "${PID_FILE}" ]] || return 1
  recorded_pid="$(<"${PID_FILE}")"
  is_expected_serena_process "${recorded_pid}"
}

# Проверяет, занят ли TCP-порт каким-либо слушающим процессом.
is_port_listening() {
  command -v lsof >/dev/null 2>&1 || return 1
  lsof -nP -iTCP:"${SERENA_PORT}" -sTCP:LISTEN >/dev/null 2>&1
}

# Находит уже запущенную подходящую Serena даже после очистки временного PID-файла.
# Предупреждает, что на порту уже работает Serena с другим контекстом.
# Контекст фиксируется при старте процесса, поэтому клиент молча получил бы чужой
# набор тулзов и чужой системный промпт; переиспользование процесса при этом
# сохраняется — решение о смене порта остаётся за человеком.
warn_on_context_mismatch() {
  local process_pid="$1"
  local process_command

  process_command="$(ps -p "${process_pid}" -o command= 2>/dev/null || true)"
  if [[ "${process_command}" == *"--context ${SERENA_CONTEXT}"* ]]; then
    return 0
  fi

  printf 'Внимание: на порту %s уже работает Serena с другим контекстом.\n' "${SERENA_PORT}" >&2
  printf 'Запрошен "%s", запущенный процесс: %s\n' "${SERENA_CONTEXT}" "${process_command}" >&2
  printf 'Контекст задаётся при старте процесса — под другой контекст нужен отдельный порт.\n' >&2
}

find_expected_listener_pid() {
  local listener_pid

  command -v lsof >/dev/null 2>&1 || return 1
  while IFS= read -r listener_pid; do
    if is_expected_serena_process "${listener_pid}"; then
      printf '%s\n' "${listener_pid}"
      return 0
    fi
  done < <(lsof -nP -t -iTCP:"${SERENA_PORT}" -sTCP:LISTEN 2>/dev/null || true)

  return 1
}

# Снимает только блокировку, принадлежащую текущему процессу скрипта.
release_launch_lock() {
  local lock_owner

  lock_owner="$(readlink "${LOCK_FILE}" 2>/dev/null || true)"
  [[ "${lock_owner}" == "$$" ]] || return 0
  rm -f -- "${LOCK_FILE}"
}

# Атомарно блокирует проверку и запуск, чтобы параллельные хуки не создали две Serena.
acquire_launch_lock() {
  local attempt
  local lock_owner=""
  local stale_lock_file="${LOCK_FILE}.stale.$$"

  for attempt in {1..100}; do
    if ln -s "$$" "${LOCK_FILE}" 2>/dev/null; then
      return 0
    fi

    lock_owner="$(readlink "${LOCK_FILE}" 2>/dev/null || true)"
    if [[ "${lock_owner}" =~ ^[0-9]+$ ]] && kill -0 "${lock_owner}" 2>/dev/null; then
      sleep 0.1
      continue
    fi

    if mv -- "${LOCK_FILE}" "${stale_lock_file}" 2>/dev/null; then
      rm -f -- "${stale_lock_file}"
    fi
    sleep 0.1
  done

  printf 'Ошибка: не удалось получить блокировку запуска Serena: %s\n' "${LOCK_FILE}" >&2
  return 1
}

# Проверяет наличие программ, необходимых для запуска и диагностики Serena.
check_dependencies() {
  SERENA_EXECUTABLE="$(command -v "${SERENA_BIN}" 2>/dev/null || true)"
  if [[ -z "${SERENA_EXECUTABLE}" ]]; then
    printf 'Ошибка: команда Serena не найдена: %s\n' "${SERENA_BIN}" >&2
    printf 'Укажите путь через SERENA_BIN или установите Serena.\n' >&2
    return 1
  fi

  if ! command -v curl >/dev/null 2>&1; then
    printf 'Ошибка: curl необходим для проверки готовности MCP-сервера.\n' >&2
    return 1
  fi
}

# Проверяет наличие stdio-прокси, запускаемого самим Codex.
check_stdio_bridge_dependencies() {
  if ! command -v "${NPX_BIN}" >/dev/null 2>&1; then
    printf 'Ошибка: npx необходим для запуска stdio-моста: %s\n' "${NPX_BIN}" >&2
    return 1
  fi
}

# Проверяет, что PID принадлежит stdio-мосту этого проекта и порта.
is_expected_stdio_bridge_process() {
  local process_pid="$1"
  local process_command

  [[ "${process_pid}" =~ ^[0-9]+$ ]] || return 1
  kill -0 "${process_pid}" 2>/dev/null || return 1
  process_command="$(ps -p "${process_pid}" -o command= 2>/dev/null || true)"

  [[ "${process_command}" == *"${LAUNCHER_SCRIPT_PATH}"* ]]
  [[ "${process_command}" == *"--stdio-bridge"* ]]
  [[ "${process_command}" == *"--port ${SERENA_PORT}"* ]]
  [[ "${process_command}" == *"--project ${PROJECT_DIR}"* ]]
}

# Регистрирует текущий stdio-мост как активного пользователя общей Serena.
register_stdio_bridge_lease() {
  mkdir -p -- "${LEASE_DIR}"
  BRIDGE_LEASE_FILE="${LEASE_DIR}/$$"
  printf '%s\n' "${SERENA_URL}" >"${BRIDGE_LEASE_FILE}"
}

# Удаляет lease, принадлежащий текущему stdio-мосту.
clear_stdio_bridge_lease() {
  [[ -n "${BRIDGE_LEASE_FILE}" ]] || return 0
  [[ "${BRIDGE_LEASE_FILE}" == "${LEASE_DIR}/$$" ]] || return 0
  rm -f -- "${BRIDGE_LEASE_FILE}"
  BRIDGE_LEASE_FILE=""
}

# Возвращает успех при наличии живого stdio-моста и очищает протухшие leases.
has_active_stdio_bridges() {
  local lease_file
  local process_pid

  [[ -d "${LEASE_DIR}" ]] || return 1
  for lease_file in "${LEASE_DIR}"/*; do
    [[ -e "${lease_file}" ]] || return 1
    process_pid="${lease_file##*/}"
    if is_expected_stdio_bridge_process "${process_pid}"; then
      return 0
    fi
    rm -f -- "${lease_file}"
  done

  return 1
}

# Проверяет, что PID-файл указывает на watchdog этого проекта и порта.
is_recorded_watchdog_running() {
  local watchdog_pid
  local process_command

  [[ -f "${WATCHDOG_PID_FILE}" ]] || return 1
  watchdog_pid="$(<"${WATCHDOG_PID_FILE}")"
  [[ "${watchdog_pid}" =~ ^[0-9]+$ ]] || return 1
  kill -0 "${watchdog_pid}" 2>/dev/null || return 1
  process_command="$(ps -p "${watchdog_pid}" -o command= 2>/dev/null || true)"

  [[ "${process_command}" == *"${SCRIPT_PATH}"* || "${process_command}" == *"${WATCHDOG_SCRIPT_PATH}"* ]]
  [[ "${process_command}" == *"--idle-watchdog"* ]]
  [[ "${process_command}" == *"--port ${SERENA_PORT}"* ]]
  [[ "${process_command}" == *"--project ${PROJECT_DIR}"* ]]
}

# Удаляет PID-файл только для текущего watchdog.
clear_current_watchdog_pid_file() {
  local recorded_pid

  [[ -f "${WATCHDOG_PID_FILE}" ]] || return 0
  recorded_pid="$(<"${WATCHDOG_PID_FILE}")"
  [[ "${recorded_pid}" == "$$" ]] || return 0
  rm -f -- "${WATCHDOG_PID_FILE}"
}

# Удаляет PID-файл, только если он принадлежит указанному процессу.
clear_pid_file_for() {
  local process_pid="$1"
  local recorded_pid

  [[ -f "${PID_FILE}" ]] || return 0
  recorded_pid="$(<"${PID_FILE}")"
  [[ "${recorded_pid}" == "${process_pid}" ]] || return 0
  rm -f -- "${PID_FILE}"
}

# Ждёт готовности endpoint и отслеживает преждевременное завершение процесса.
wait_until_ready() {
  local process_pid="$1"
  local attempt

  for attempt in {1..60}; do
    if ! kill -0 "${process_pid}" 2>/dev/null; then
      clear_pid_file_for "${process_pid}"
      printf 'Ошибка: Serena завершилась до готовности endpoint.\n' >&2
      return 1
    fi

    if is_serena_endpoint_ready; then
      return 0
    fi
    sleep 0.5
  done

  printf 'Ошибка: Serena не стала доступна за 30 секунд.\n' >&2
  printf 'Последние строки лога %s:\n' "${LOG_FILE}" >&2
  tail -n 30 -- "${LOG_FILE}" >&2 || true
  return 1
}

# Отбрасывает шумный WARNING dart-LSP и дописывает остальной поток в лог.
filter_serena_output() {
  grep --line-buffered -vF "${LOG_NOISE_PATTERN}" | tee -a "${LOG_FILE}"
}

# Формирует общие аргументы процесса Serena.
build_serena_arguments() {
  SERENA_ARGUMENTS=(
    start-mcp-server
    --transport streamable-http
    --host "${SERENA_HOST}"
    --port "${SERENA_PORT}"
    --context "${SERENA_CONTEXT}"
    --project "${PROJECT_DIR}"
    --enable-web-dashboard false
    --open-web-dashboard false
  )
}

# Ждёт появления подходящего слушателя после запуска через системный менеджер процессов.
wait_until_listener_ready() {
  local attempt
  local listener_pid=""

  for attempt in {1..60}; do
    listener_pid="$(find_expected_listener_pid || true)"
    if [[ -n "${listener_pid}" ]] && is_serena_endpoint_ready; then
      SERENA_PID="${listener_pid}"
      printf '%s\n' "${SERENA_PID}" >"${PID_FILE}"
      return 0
    fi
    sleep 0.5
  done

  printf 'Ошибка: Serena не стала доступна за 30 секунд.\n' >&2
  printf 'Последние строки лога %s:\n' "${LOG_FILE}" >&2
  tail -n 30 -- "${LOG_FILE}" >&2 || true
  return 1
}

# Запускает Serena как временную launchd-задачу, не устанавливая автозапуск.
start_serena_with_launchd() {
  # Удаляется только завершённая задача с уникальной меткой текущего проекта и порта.
  launchctl remove "${LAUNCHD_LABEL}" >/dev/null 2>&1 || true
  launchctl submit \
    -l "${LAUNCHD_LABEL}" \
    -o "${LOG_FILE}" \
    -e "${LOG_FILE}" \
    -- /usr/bin/env "PATH=${LAUNCH_ENV_PATH}" "${SERENA_EXECUTABLE}" "${SERENA_ARGUMENTS[@]}"

  wait_until_listener_ready
}

# Останавливает только общий background-процесс Serena текущего проекта и порта.
stop_shared_serena_background() {
  local listener_pid=""

  listener_pid="$(find_expected_listener_pid || true)"
  if [[ "$(uname -s)" == "Darwin" ]] && command -v launchctl >/dev/null 2>&1; then
    launchctl remove "${LAUNCHD_LABEL}" >/dev/null 2>&1 || true
  elif [[ -n "${listener_pid}" ]]; then
    kill -TERM "${listener_pid}" 2>/dev/null || true
  fi

  if [[ -n "${listener_pid}" ]]; then
    clear_pid_file_for "${listener_pid}"
  fi
}

# Наблюдает за leases и останавливает Serena после заданного периода простоя.
run_idle_watchdog() {
  local idle_started_at=""
  local current_time

  printf '%s\n' "$$" >"${WATCHDOG_PID_FILE}"
  trap clear_current_watchdog_pid_file EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  trap 'exit 129' HUP

  while true; do
    if has_active_stdio_bridges; then
      idle_started_at=""
      sleep 10
      continue
    fi

    current_time="$(date +%s)"
    if [[ -z "${idle_started_at}" ]]; then
      idle_started_at="${current_time}"
    elif ((current_time - idle_started_at >= IDLE_TIMEOUT_SECONDS)); then
      # Lock и повторная проверка закрывают гонку с новой сессией на границе таймаута.
      acquire_launch_lock
      if ! has_active_stdio_bridges; then
        stop_shared_serena_background
      fi
      release_launch_lock
      idle_started_at="${current_time}"
    fi

    sleep 10
  done
}

# Гарантирует запуск одного watchdog для проекта и порта.
ensure_idle_watchdog() {
  [[ "${IDLE_TIMEOUT_SECONDS}" != "0" ]] || return 0

  if [[ "$(uname -s)" == "Darwin" ]] && command -v launchctl >/dev/null 2>&1; then
    if launchctl print "gui/$(id -u)/${WATCHDOG_LAUNCHD_LABEL}" >/dev/null 2>&1; then
      return 0
    fi

    # launchd не может напрямую читать исполняемый скрипт из Documents без TCC-доступа.
    cp "${SCRIPT_PATH}" "${WATCHDOG_SCRIPT_PATH}"
    chmod 700 "${WATCHDOG_SCRIPT_PATH}"
    launchctl submit \
      -l "${WATCHDOG_LAUNCHD_LABEL}" \
      -o "${LOG_FILE}" \
      -e "${LOG_FILE}" \
      -- /usr/bin/env "PATH=${LAUNCH_ENV_PATH}" /bin/bash "${WATCHDOG_SCRIPT_PATH}" \
      --idle-watchdog \
      --launcher-script "${LAUNCHER_SCRIPT_PATH}" \
      --project "${PROJECT_DIR}" \
      --port "${SERENA_PORT}" \
      --host "${SERENA_HOST}" \
      --context "${SERENA_CONTEXT}" \
      --idle-timeout "${IDLE_TIMEOUT_SECONDS}"
    return 0
  fi

  if is_recorded_watchdog_running; then
    return 0
  fi

  nohup "${SCRIPT_PATH}" \
    --idle-watchdog \
    --project "${PROJECT_DIR}" \
    --port "${SERENA_PORT}" \
    --host "${SERENA_HOST}" \
    --context "${SERENA_CONTEXT}" \
    --idle-timeout "${IDLE_TIMEOUT_SECONDS}" \
    >>"${LOG_FILE}" 2>&1 </dev/null &
}

# Завершает дочерний stdio-прокси и снимает lease текущей сессии.
cleanup_stdio_bridge() {
  if [[ "${BRIDGE_PROXY_PID}" =~ ^[0-9]+$ ]] && kill -0 "${BRIDGE_PROXY_PID}" 2>/dev/null; then
    kill -TERM "${BRIDGE_PROXY_PID}" 2>/dev/null || true
    wait "${BRIDGE_PROXY_PID}" 2>/dev/null || true
  fi
  BRIDGE_PROXY_PID=""
  clear_stdio_bridge_lease
}

# Гарантирует единственный HTTP-инстанс Serena и держит stdio-мост до закрытия сессии.
start_stdio_bridge() {
  local bridge_exit_code

  check_stdio_bridge_dependencies
  register_stdio_bridge_lease
  trap cleanup_stdio_bridge EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  trap 'exit 129' HUP

  # Весь служебный stdout скрывается: он зарезервирован протоколом MCP.
  "${SCRIPT_PATH}" \
    --background \
    --project "${PROJECT_DIR}" \
    --port "${SERENA_PORT}" \
    --host "${SERENA_HOST}" \
    --context "${SERENA_CONTEXT}" \
    --idle-timeout "${IDLE_TIMEOUT_SECONDS}" \
    >/dev/null

  "${NPX_BIN}" -y "${MCP_REMOTE_PACKAGE}" \
    "${SERENA_URL}" \
    --allow-http \
    --transport http-only \
    --silent <&0 &
  BRIDGE_PROXY_PID="$!"

  if wait "${BRIDGE_PROXY_PID}"; then
    bridge_exit_code=0
  else
    bridge_exit_code="$?"
  fi
  BRIDGE_PROXY_PID=""
  return "${bridge_exit_code}"
}

# Запускает Serena независимо от терминала и возвращается после готовности endpoint.
start_serena_background() {
  build_serena_arguments
  : >"${LOG_FILE}"

  if [[ "$(uname -s)" == "Darwin" ]] && command -v launchctl >/dev/null 2>&1; then
    start_serena_with_launchd
  else
    nohup "${SERENA_BIN}" "${SERENA_ARGUMENTS[@]}" >>"${LOG_FILE}" 2>&1 </dev/null &
    SERENA_PID="$!"
    printf '%s\n' "${SERENA_PID}" >"${PID_FILE}"
    wait_until_ready "${SERENA_PID}"
  fi

  ensure_idle_watchdog
  printf 'Serena запущена в фоне. PID: %s, URL: %s\n' "${SERENA_PID}" "${SERENA_URL}"
  printf 'Лог: %s\n' "${LOG_FILE}"
}

# Запускает Serena как дочерний процесс и остаётся привязанным к терминалу.
start_serena_foreground() {
  local exit_code

  build_serena_arguments
  : >"${LOG_FILE}"

  "${SERENA_BIN}" "${SERENA_ARGUMENTS[@]}" > >(filter_serena_output) 2>&1 &
  SERENA_PID="$!"
  printf '%s\n' "${SERENA_PID}" >"${PID_FILE}"

  wait_until_ready "${SERENA_PID}"
  printf '\nSerena успешно запущена. PID: %s\n' "${SERENA_PID}"
  printf 'Лог: %s\n' "${LOG_FILE}"
  print_codex_config
  printf '\nSerena работает. Нажмите Ctrl+C для остановки.\n'

  if wait "${SERENA_PID}"; then
    exit_code=0
  else
    exit_code="$?"
  fi
  clear_pid_file_for "${SERENA_PID}"
  SERENA_PID=""
  printf 'Serena завершена. Код: %s\n' "${exit_code}"
  return "${exit_code}"
}

# Останавливает только foreground-процесс, принадлежащий текущему лаунчеру.
stop_foreground_child() {
  [[ "${BACKGROUND_MODE}" == false ]] || return 0
  [[ "${SERENA_PID}" =~ ^[0-9]+$ ]] || return 0

  if kill -0 "${SERENA_PID}" 2>/dev/null; then
    kill -TERM "${SERENA_PID}" 2>/dev/null || true
    wait "${SERENA_PID}" 2>/dev/null || true
  fi
  clear_pid_file_for "${SERENA_PID}"
  SERENA_PID=""
}

# Освобождает foreground-процесс и блокировку при завершении лаунчера.
cleanup_on_exit() {
  stop_foreground_child
  release_launch_lock
}

# Обрабатывает закрытие терминала и интерактивные сигналы остановки.
handle_signal() {
  local signal_name="$1"
  local exit_code="$2"

  printf '\nПолучен сигнал %s. Останавливаю foreground Serena...\n' "${signal_name}"
  exit "${exit_code}"
}

# Выполняет идемпотентную проверку и запускает выбранный режим.
main() {
  local existing_listener_pid=""
  local recorded_pid

  parse_arguments "$@"
  initialize_runtime
  mkdir -p -- "${RUNTIME_DIR}" "${LEASE_DIR}"

  if [[ "${IDLE_WATCHDOG_MODE}" == true ]]; then
    [[ "${IDLE_TIMEOUT_SECONDS}" != "0" ]] || return 0
    run_idle_watchdog
  fi

  if [[ "${STDIO_BRIDGE_MODE}" == true ]]; then
    start_stdio_bridge
  fi

  check_dependencies
  acquire_launch_lock
  trap cleanup_on_exit EXIT
  trap 'handle_signal INT 130' INT
  trap 'handle_signal TERM 143' TERM
  trap 'handle_signal HUP 129' HUP

  if is_recorded_serena_running; then
    recorded_pid="$(<"${PID_FILE}")"
    warn_on_context_mismatch "${recorded_pid}"
    if [[ "${BACKGROUND_MODE}" == true ]]; then
      wait_until_ready "${recorded_pid}"
      ensure_idle_watchdog
      printf 'Serena уже запущена. PID: %s, URL: %s\n' "${recorded_pid}" "${SERENA_URL}"
      return 0
    fi

    printf 'Ошибка: Serena для этого проекта и порта уже запущена (PID %s).\n' "${recorded_pid}" >&2
    return 1
  fi

  rm -f -- "${PID_FILE}"
  existing_listener_pid="$(find_expected_listener_pid || true)"
  if [[ -n "${existing_listener_pid}" ]]; then
    printf '%s\n' "${existing_listener_pid}" >"${PID_FILE}"
    warn_on_context_mismatch "${existing_listener_pid}"
    if [[ "${BACKGROUND_MODE}" == true ]]; then
      wait_until_ready "${existing_listener_pid}"
      ensure_idle_watchdog
      printf 'Serena уже запущена. PID: %s, URL: %s\n' "${existing_listener_pid}" "${SERENA_URL}"
      return 0
    fi

    printf 'Ошибка: Serena для этого проекта и порта уже запущена (PID %s).\n' "${existing_listener_pid}" >&2
    return 1
  fi

  if is_port_listening || is_serena_endpoint_ready; then
    printf 'Ошибка: TCP-порт %s уже занят другим процессом.\n' "${SERENA_PORT}" >&2
    printf 'Передайте другой порт через --port или SERENA_MCP_PORT.\n' >&2
    return 1
  fi

  if [[ "${BACKGROUND_MODE}" == true ]]; then
    start_serena_background
  else
    start_serena_foreground
  fi
}

main "$@"
