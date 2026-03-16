# Controlador Mesa

## Compilar

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 main --build-path build/esp32.esp32.esp32
```

## Upload

```bash
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyACM0 main --input-dir build/esp32.esp32.esp32
```

## Backup da flash (antes de subir novo firmware)

```bash
~/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool --port /dev/ttyACM0 read_flash 0x0 0x400000 backup_firmware.bin
```

## Restaurar backup

```bash
~/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool --port /dev/ttyACM0 write_flash 0x0 backup_firmware.bin
```
