# MSB Combat (Qwen) — программа STM32 платы МСБ

Основное приложение платы **МСБ (плата коммутации режимов)** на микроконтроллере
**STM32F411RE**. Обеспечивает обмен между баллистическим компьютером (БК/ВС) и
платой ввода-вывода: опрос дискретных входов и выдача дискретных выходов через
ПЛИС, обмен по линии RS-485/RS-422, отладочный терминал eAssist/DSPA по UART.

Адресация приложения рассчитана на работу вместе с загрузчиком
[msb_bootloader](https://github.com/auramenka-it90/stm32_msb_bootloader):
область приложения — секторы 5–6, `0x08020000 … 0x0805FFFF` (256 Кбайт).
Режим работы без загрузчика описан в разделе «Режимы сборки».

## Состав платы, с которым работает программа

| Узел | Подключение |
| :--- | :--- |
| ПЛИС Xilinx Spartan-6 (31 вход / 8 выходов, коммутатор RS-485) | SPI1 (регистровый мост, 3 устройства), линия IRQ |
| Линия обмена с ВС | USART2 → приёмопередатчики RS-485/RS-422 (DD19/DD20, выбор канала регистром ПЛИС) |
| Терминал eAssist/DSPA | USART1 + DMA |
| ОСРВ | FreeRTOS (CMSIS-RTOS v2) |

## Структура репозитория

| Путь | Назначение |
| :--- | :--- |
| `Core/` | генерация CubeIDE: main, FreeRTOS, периферия |
| `board_support_package/board_support_package.[ch]` | конфигурация BSP, флаг `APPLICATION_USE_BOOTLOADER` |
| `board_support_package/fire_control_system/` | `fcs.c` — рабочий цикл 50 Гц: чтение STATUS ПЛИС, запись приказов BC в `FCS_CONTROL`, контроль сторожа выходов |
| `board_support_package/fpga/` | `fpga_control.c`, `fpga_reg_map.h`, `stm32_2_fpga_spi_bridge/` — драйвер SPI-моста ПЛИС |
| `board_support_package/host/` | `host.c` — протокольный обмен с ВС (RS-485) |
| `board_support_package/terminal/` | терминал eAssist: `dspa`, `terminal_signals`, `wrapper`, `version_control` (встраивание версии из git) |
| `board_support_package/memory/` | внутренняя Flash (конфигурация, служебные данные) |
| `board_support_package/pin_management/` | дескрипторы выводов + `gpio_fallbacks.h` |
| `board_support_package/std_com/` | транспорт UART + DMA |
| `Middlewares/`, `Drivers/` | FreeRTOS, HAL/CMSIS |
| `msb_combat_qwen.ioc` | проект CubeIDE (STM32F411RE) |
| `STM32F411RETX_FLASH.ld` | файл сегментации: без загрузчика (0x08000000, 512 К) |
| `STM32F411RETX_FLASH_BL.ld` | файл сегментации: под загрузчик (0x08020000, 256 К) |
| `patch_crc.py`, `patch_crc.bat` | подготовка образа: заполнение размера и CRC32 |

## Алгоритм работы (кратко)

1. Инициализация (`fcs_init_fpga_hardware`): SPI-мост, выбор канала RS-485
   (DD19/DD20), период выборки 10 мс, маски инверсии входов, режим
   выборки-хранения 100 Гц, сторож выходов; прерывания от ПЛИС запрещены,
   контроль выполняется опросом.
2. Цикл FCS с периодом 50 Гц: чтение STATUS_1/2 → передача состояний входов
   в протоколе MSB→BC; приём пакетов BC → немедленная запись `FCS_CONTROL`;
   каждые 20 мс — сервисная запись, перезапускающая сторож выходов.
3. Протокольная логика полностью в STM32; ПЛИС выполняет только функции
   регистрового ввода-вывода.

## Режимы сборки

В файле `board_support_package/board_support_package.h` задан переключатель:

```c
#define APPLICATION_USE_BOOTLOADER      1
```

**Режим 1 — работа с загрузчиком (основной, по умолчанию):**

- `APPLICATION_USE_BOOTLOADER = 1`;
- файл сегментации `STM32F411RETX_FLASH_BL.ld` (адрес `0x08020000`, 256 К);
  в конфигурации `Debug` он задан в свойствах проекта (MCU Compiler →
  Linker Script). Внимание: путь записан абсолютный; при переносе проекта на
  другую машину заменить на `${workspace_loc:/${ProjName}/STM32F411RETX_FLASH_BL.ld}`;
- при этом значении флага в `Core/Src/main.c` в секцию `.fw_header`
  размещается «паспорт прошивки» (`magic 0x55AA55AA`, `version 1`, поля
  `fw_size` и `crc32` — заполнители `0xFFFFFFFF`), а в `main()`
  устанавливается `SCB->VTOR = 0x08020000U`;
- порядок подготовки образа:
  1. сборка конфигурации `Debug` → `Debug\msb_combat_qwen.bin`;
  2. заполнение паспорта: `patch_crc.bat Debug\msb_combat_qwen.bin`.
     Скрипт находит сигнатуру `55AA55AA`, дополняет образ байтами `0xFF` до
     кратности 4, вычисляет CRC32 аппаратным алгоритмом STM32 (полином
     `0x04C11DB7`, старший бит первый, начальное значение `0xFFFFFFFF`) по
     данным после 16-байтного заголовка и записывает полный размер и CRC
     в заголовок;
  3. передача пропатченного `.bin` плате через терминал eAssist. Загрузчик
     проверяет размер и CRC, записывает образ в секторы 5–6 и выполняет
     переход по нему.

**Режим 2 — прямое программирование, без загрузчика:**

- в `board_support_package.h` установить `APPLICATION_USE_BOOTLOADER = 0`
  (или закомментировать `#define`); при этом «паспорт прошивки» в образ не
  включается, сдвиг таблицы векторов не выполняется (сброс VTOR не требуется,
  программа начинается с `0x08000000`);
- файл сегментации `STM32F411RETX_FLASH.ld` (адрес `0x08000000`, 512 К);
  он задан в конфигурации `Release`;
- прошивка выполняется отладчиком (ST-LINK) по SWD напрямую; операция
  заполнения CRC не требуется.

## Связанные проекты

* ПЛИС этой платы: [fpga_msb_combat](https://github.com/auramenka-it90/fpga_msb_combat)
* Загрузчик этой платы: [stm32_msb_bootloader](https://github.com/auramenka-it90/stm32_msb_bootloader)
* Баллистический компьютер: [stm32_bc_combat](https://github.com/auramenka-it90/stm32_bc_combat),
  [stm32_bc_bootloader](https://github.com/auramenka-it90/stm32_bc_bootloader)
