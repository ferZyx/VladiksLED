Build firmware: pio run -e d1_mini
Clean rebuild: pio run -e d1_mini -t clean && pio run -e d1_mini
Upload firmware: pio run -e d1_mini -t upload
Serial monitor: pio device monitor -b 115200
Git status/diff/log for VCS checks.
Serena MCP: общий лаунчер ./start-shared-serena.sh (копия из lombard). Порты: 9147 — Codex (.codex/config.toml), 9148 — Claude Code (.mcp.json). Оба конфига локальные и в .gitignore. Ручная диагностика: ./start-shared-serena.sh --background --port 9148 --context claude-code; логи в /tmp/shared-serena/<hash>/.
Tooling on dev Mac: PlatformIO CLI installed via pipx (`pipx install platformio`, binary ~/.local/bin/pio). Deps install without building: pio pkg install -e d1_mini.
Size analysis: ~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-size -A .pio/build/d1_mini/firmware.elf and xtensa-lx106-elf-nm -S --size-sort -C on the same elf.
