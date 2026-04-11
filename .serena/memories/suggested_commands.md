Build firmware: pio run -e d1_mini
Clean rebuild: pio run -e d1_mini -t clean && pio run -e d1_mini
Upload firmware: pio run -e d1_mini -t upload
Serial monitor: pio device monitor -b 115200
Git status/diff/log for VCS checks.