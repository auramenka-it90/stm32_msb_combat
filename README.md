# MSB Combat (Qwen) — прошивка STM32 платы МСБ

Основное приложение платы **МСБ (плата коммутации режимов)** на **STM32F411RE**.
Связующее звено между баллистическим компьютером (БК/ВС) и платой ввода-вывода:
крутит тракт «дискретные входы → протокол → приказы → дискретные выходы»
через FPGA и линию RS‑485/RS‑422, плюс отладочный терминал eAssist/DSPA.

Загружается в область **секторов 5–6 (0x08020000 … 0x0805FFFF, 256 К)**
загрузчиком [msb_bootloader](https://github.com/auramenka-it90/stm32_msb_bootloader);
для «голого» чипа есть отдельный линковщик без загрузчика (см. «Сборка»).

## Состав платы, с которым работает прошивка

| Узел | Подключение |
| :--- | :--- |
| FPGA Xilinx Spartan‑6 (31 вход / 8 выходов, RS‑485 mux) | SPI1 (реестровый мост, 3 устройства), IRQ‑линия |
| Линия обмена с ВС | USART2 → RS‑485/RS‑422 (DD19/DD20, выбор канала регистром FPGA) |
| Терминал eAssist/DSPA | USART1 + DMA (разъём 221В34Ф26) |
| RTOS | FreeRTOS (CMSIS‑RTOS v2) |

## Структура репозитория

```
C_QWEN/
├── Core/                       CubeIDE-генерация, main, FreeRTOS, периферия
├── board_support_package/
│   ├── board_support_package.[ch]   реестр BSP и конфигурация
│   ├── fire_control_system/         fcs.c — рабочий цикл 50 Гц: STATUS FPGA,
│   │                                приказы BC -> FCS_CONTROL, heartbeat сторожа
│   ├── fpga/                        fpga_control.c, fpga_reg_map.h,
│   │                                stm32_2_fpga_spi_bridge/ — SPI-мост ПЛИС
│   ├── host/                        host.c — протокольный обмен с ВС (RS-485)
│   ├── terminal/                    eAssist: dspa, terminal_signals, wrapper,
│   │                                version_control (git-инжект версии)
│   ├── memory/                      внутренняя Flash (конфиг/служебные данные)
│   ├── pin_management/              дескрипторы пинов + gpio_fallbacks.h
│   └── std_com/                     транспорт UART+DMA
├── Middlewares/, Drivers/      FreeRTOS, HAL/CMSIS
├── msb_combat_qwen.ioc         проект CubeIDE (STM32F411RE)
├── STM32F411RETX_FLASH.ld      линковка «сам по себе» (0x08000000, 512K)
└── STM32F411RETX_FLASH_BL.ld   линковка под загрузчик (0x08020000, 256K)
```

## Как это работает (в двух словах)

1. **Инициализация** (`fcs_init_fpga_hardware`): SPI-мост, выбор UART-канала
   (DD19/DD20), период выборки 10 мс, маски инверсии входов, S/H 100 Гц,
   сторож выходов, прерывания FPGA — запрещены (поллирование).
2. **Цикл FCS, жёсткие 50 Гц**: чтение STATUS_1/2 → передача входов в протокол
   MSB→BC; приём пакетов BC → немедленная запись `FCS_CONTROL` (выходы
   реагируют за миллисекунды); каждые 20 мс — heartbeat сторожа выходов.
3. **Логика вся в STM32**, ПЛИС — «глупые и мгновенные» регистры.

## Сборка и подготовка образа к загрузке

**Конфигурации линковки (проверено в `.cproject`):**

| Конфигурация CubeIDE | Линкер | Адрес/размер | Назначение |
| :--- | :--- | :--- | :--- |
| `Debug` | `STM32F411RETX_FLASH_BL.ld` | `0x08020000`, 256 К | приложение **под загрузчик** (прошивка через терминал) |
| `Release` | `STM32F411RETX_FLASH.ld` | `0x08000000`, 512 К | «сам по себе», прошивка ST‑LINK целиком |

⚠️ В конфигурации `Debug` линкер прописан **абсолютным путём** (`D:\...`);
на другой машине замените на
`${workspace_loc:/${ProjName}/STM32F411RETX_FLASH_BL.ld}`
(формат уже используется в `Release`).

**Порядок для загрузки через загрузчик (CRC-патч обязателен, у загрузчика `BSP_CHACK_APPL_CRC=1`):**

1. CubeIDE → конфигурация `Debug` → Build → `Debug\msb_combat_qwen.bin`.
2. Прогнать патчер (сам находит Python 3):
   ```bat
   patch_crc.bat Debug\msb_combat_qwen.bin
   ```
   `patch_crc.py` ищет в образе заголовок‑сигнатуру `55AA55AA, ver=1, size=FFFFFFFF,
   crc=FFFFFFFF` (16 байт), дополняет образ 0xFF до кратности 4, считает **аппаратный
   CRC32 STM32** (полином 0x04C11DB7, MSB‑first, init 0xFFFFFFFF) по всему, что идёт
   **после** заголовка, и вписывает в заголовок полный размер и CRC.
3. Пропатченный `.bin` передать плате через терминал eAssist: загрузчик проверит
   размер+CRC, запишет в секторы 5–6 и выполнит безопасный прыжок.

**Для чистого чипа (ST‑LINK):** конфигурация `Release` (старт с `0x08000000`),
прошивать `.elf`/`.bin` напрямую, CRC‑патч не нужен.

После регенерации `.ioc` всегда проверяйте, какой `.ld` активен в конфигурации сборки.

## Связанные проекты

* ПЛИС этой платы: [fpga_msb_combat](https://github.com/auramenka-it90/fpga_msb_combat)
* Загрузчик этой платы: [stm32_msb_bootloader](https://github.com/auramenka-it90/stm32_msb_bootloader)
* Баллистический компьютер (партия): [stm32_bc_combat](https://github.com/auramenka-it90/stm32_bc_combat),
  [stm32_bc_bootloader](https://github.com/auramenka-it90/stm32_bc_bootloader)
