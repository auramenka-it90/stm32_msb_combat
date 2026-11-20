# Архитектура защищенного Bootloader & OTA для STM32

Данный документ описывает процесс настройки двух проектов (Bootloader и Main Application) для обеспечения безопасной загрузки прошивки, проверки целостности данных (CRC32) и предотвращения запуска поврежденного кода.

---

## ЧАСТЬ 1: Настройка основной прошивки (Main Application)

### 1. Сдвиг адреса и защита заголовка (Файл .ld)
Откройте файл скрипта линкера `STM32F411RETX_FLASH.ld`.

**А) Сдвигаем начало флэш-памяти:**
Стартовый адрес слота приложения — `0x08020000` (секторы 5–6, зарезервированы загрузчиком `msb_bootloader`), размер — ровно `256K` (больше загрузчик не примет: `FLASH_CPU_SIZE = 256K`). У STM32F411 нет CCMRAM, её в блоке быть не должно.

```text
MEMORY
{
  RAM       (xrw)    : ORIGIN = 0x20000000,   LENGTH = 128K
  FLASH     (rx)     : ORIGIN = 0x08020000,   LENGTH = 256K /* Слот приложения: секторы 5..6 */
}
```

**Б) Создаем секцию для паспорта прошивки:**
В блоке `SECTIONS`, сразу после таблицы векторов прерываний (`.isr_vector`), добавьте новую секцию `.fw_header`. Команда `KEEP` обязательна, чтобы оптимизатор не удалил структуру.

```text
SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* Секция паспорта прошивки (ОБЯЗАТЕЛЬНО ЗДЕСЬ) */
  .fw_header :
  {
    . = ALIGN(4);
    KEEP(*(.fw_header))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
```

### 2. Объявление паспорта и перенос векторов (main.c)
Добавьте структуру в глобальную область и перенесите вектора при старте программы.

```c
/* USER CODE BEGIN 0 */
typedef struct {
    uint32_t magic;      // Магическое слово (0x55AA55AA)
    uint32_t version;    // Версия прошивки
    uint32_t fw_size;    // Размер бинарника (заполнит скрипт)
    uint32_t crc32;      // CRC32 данных (заполнит скрипт)
} fw_header_t;

/* Размещение структуры строго в секции .fw_header */
__attribute__((section(".fw_header"), used)) volatile const fw_header_t fw_header = {
    .magic   = 0x55AA55AA,  
    .version = 1,         
    .fw_size = 0xFFFFFFFF, 
    .crc32   = 0xFFFFFFFF  
};
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  SCB->VTOR = 0x08020000U; // Адрес начала Main App (в проекте включается флагом APPLICATION_USE_BOOTLOADER==1)
  __enable_irq();          // Включаем прерывания после Bootloader'а
  /* USER CODE END 1 */
  
  HAL_Init();
  // ...
```

---

## ЧАСТЬ 2: Автоматизация подписи (Python-скрипт)

Создайте файл `patch_crc.py` в корневой папке проекта Main App. Скрипт находит структуру, выравнивает размер файла кратно 4 байтам и вычисляет аппаратно-совместимый CRC32.

```python
import sys
import struct
import os

# Аппаратный алгоритм CRC32 контроллеров STM32
def stm32_crc32(data):
    crc = 0xFFFFFFFF
    for i in range(0, len(data), 4):
        word = struct.unpack('<I', data[i:i+4])[0]
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def patch_bin(filepath):
    print(f"--- Starting CRC patch for: {os.path.basename(filepath)} ---")
    
    with open(filepath, 'rb') as f:
        data = bytearray(f.read())

    # Ищем: Magic(55AA55AA) + Version(1) + Size(FFFFFFFF) + CRC(FFFFFFFF)
    signature = struct.pack('<IIII', 0x55AA55AA, 1, 0xFFFFFFFF, 0xFFFFFFFF)
    header_offset = data.find(signature)
    
    if header_offset == -1:
        print("ERROR: fw_header signature not found in binary!")
        sys.exit(1)
        
    print(f"Header found at offset: 0x{header_offset:04X}")

    # Выравнивание кратно 4 байтам
    padding_needed = (4 - (len(data) % 4)) % 4
    if padding_needed > 0:
        data.extend(b'\xFF' * padding_needed)

    total_size = len(data)
    
    # Данные для расчета CRC (все, что после 16 байт заголовка)
    payload_offset = header_offset + 16
    payload_data = data[payload_offset:]
    
    calculated_crc = stm32_crc32(payload_data)
    
    print(f"Total file size: {total_size} bytes")
    print(f"Calculated CRC32: 0x{calculated_crc:08X}")

    # Запись новых значений
    struct.pack_into('<I', data, header_offset + 8, total_size)
    struct.pack_into('<I', data, header_offset + 12, calculated_crc)

    with open(filepath, 'wb') as f:
        f.write(data)
        
    print("--- Patch successful! ---")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python patch_crc.py <path_to_bin_file>")
        sys.exit(1)
    patch_bin(sys.argv[1])
```

---

## ЧАСТЬ 3: Настройки STM32CubeIDE (Main App)

Для обхода багов многопоточной сборки Eclipse, генерация `.bin` и вызов скрипта объединяются в одну команду.

1. Правый клик по проекту -> **Properties** -> **C/C++ Build** -> **Settings**.
2. Вкладка **Tool Settings** -> **MCU Post build outputs**:
   * **СНИМИТЕ** галочку *Convert to binary file (-O binary)*.
3. Перейдите на вкладку **Build Steps**.
4. В поле **Post-build steps** -> **Command** вставьте строку:
   ```bash
   arm-none-eabi-objcopy -O binary "${BuildArtifactFileName}" "${BuildArtifactFileBaseName}.bin" && "${ProjDirPath}/patch_crc.bat" "${BuildArtifactFileBaseName}.bin"
   ```
5. Сделайте **Clean Project**, затем **Build All**. В консоли должно появиться сообщение `Patch successful!`.

---

## ЧАСТЬ 4: Bootloader (Проверка и Прыжок)

> Фактическая реализация — в проекте `msb_bootloader_qwen`
> (`board_support_package/memory/application/application.c`, `board_support_package.c`).
> Она строже приведённого ниже эталона: `check_application()` без аргумента
> (адрес из `APPLICATION_ADDRESS`), отсечение `fw_size` по размеру области
> (256K), аннулирование кэша flash перед проверкой; `jump_main_application()`
> дополнительно останавливает RTOS, деинициализирует периферии, чистит NVIC
> и гасит кэши перед прыжком.

### 1. Настройка CubeMX
В конфигураторе Bootloader'а включите модуль **CRC** (настройки по умолчанию, параметры менять не нужно). Сгенерируйте код.

### 2. Логика проверки и прыжка (`application.c`)

```c
#include "application.h"

extern CRC_HandleTypeDef hcrc;

bool check_application(uint32_t app_addr) {
    typedef struct {
        uint32_t magic;
        uint32_t version;
        uint32_t fw_size;
        uint32_t crc32;
    } fw_header_t;

    fw_header_t *fw_info = NULL;
    uint32_t *search_ptr = (uint32_t *)app_addr;

    /* 1. Поиск заголовка (первые 1024 байт) */
    for (int i = 0; i < 256; i++) {
        if (search_ptr[i] == 0x55AA55AA) {
            fw_info = (fw_header_t *)&search_ptr[i];
            break;
        }
    }

    if (fw_info == NULL || fw_info->fw_size == 0xFFFFFFFF) return false; 

    /* 2. Подготовка данных для CRC */
    uint32_t *payload_ptr = (uint32_t *)(fw_info + 1); 
    uint32_t header_offset_bytes = (uint32_t)payload_ptr - app_addr;
    
    if (fw_info->fw_size <= header_offset_bytes) return false;

    uint32_t payload_bytes = fw_info->fw_size - header_offset_bytes;
    uint32_t payload_words = payload_bytes / 4; 

    /* 3. Аппаратный расчет CRC32 */
    uint32_t calc_crc = HAL_CRC_Calculate(&hcrc, payload_ptr, payload_words);

    return (calc_crc == fw_info->crc32);
}

void jump_main_application(uint32_t addr) {
    void (*Jump_To_Application)(void);
    uint32_t JumpAddress;
    uint32_t stack_pointer = *(__IO uint32_t*)addr;

    /* Проверка валидности SP (у STM32F411 только SRAM 0x20000000..0x20020000) */
    if (((stack_pointer & 0xFF000000) == 0x20000000) || 
        ((stack_pointer & 0xFF000000) == 0x10000000)) {
        
        __disable_irq();
        
        /* Сброс периферии и SysTick */
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;

        /* Прыжок */
        JumpAddress = *(__IO uint32_t*) (addr + 4);
        Jump_To_Application = (void (*)(void)) JumpAddress;

        __set_MSP(stack_pointer);
        Jump_To_Application();
    }
}
```

---

## ⚠️ Важные правила отладки

1. **Прошивка из IDE:** При прошике Main Application отладчиком всегда выбирайте опцию **Erase Sectors**, а не *Full Chip Erase*, чтобы программатор не стер Bootloader.
2. **Тестирование защиты:** Для проверки механизма отклонения битых прошивок временно измените строку в Python-скрипте на `calc_crc = stm32_crc32(payload_data) ^ 0xFF`. Bootloader должен выявить несовпадение CRC и отказаться прыгать в приложение.

---

## Эксплуатация: переключение режимов (ручной переключатель)

Проект живёт в двух режимах. Переключатель **двухпозиционный** - флаг в коде и
скрипт линковки переключаются **только парой**:

| Режим | `APPLICATION_USE_BOOTLOADER` | Script file в Properties |
|---|---|---|
| Автономный (отладка, старт с 0x08000000) | `0` | пусто |
| Под загрузчиком (прошивка терминалом, образ с CRC) | `1` | `STM32F411RETX_FLASH_BL.ld` |

### Где переключать

1. **Флаг**: `board_support_package/board_support_package.h`
   -> `#define APPLICATION_USE_BOOTLOADER 0` (или `1`).
   Он же включает/выключает VTOR в `main.c` и паспорт в образе.
2. **Скрипт**: ПКМ по проекту -> Properties -> C/C++ Build -> Settings ->
   Tool Settings -> **MCU GCC Linker -> General -> Script file management**:
   - режим загрузчика: `Add...` -> `STM32F411RETX_FLASH_BL.ld`;
   - автономный: выделить строку -> `Remove`.
   Затем Apply and Close и **Project -> Clean -> Build**.

### Post-build (настроен один раз, не переключается)

Properties -> C/C++ Build -> Build Steps -> **After build** (вписано в `.cproject`
для Debug и Release; при восстановлении из GUI вставить одной строкой):

```bat
arm-none-eabi-objcopy -O binary "${BuildArtifactFileName}" "${BuildArtifactFileBaseName}.bin" && "${ProjDirPath}/patch_crc.bat" "${BuildArtifactFileBaseName}.bin" || echo [patch skipped: standalone build, no fw_header in image]
```

Команда всеядна: в режиме загрузчика патчит образ, в автономном - печатает
`ERROR: fw_header signature not found` и `[patch skipped...]`, **сборка считается
успешной**. Строка `ERROR: fw_header signature not found` в консоли автономной
сборки - норма, а не ошибка.

### Признаки правильности после Build

- Режим загрузчика: в консоли `Header found at offset: 0x...` и
  `Patch successful!`; в `.bin` на смещении ~0x198 байты `08 55 AA 55`;
  Reset-вектор образа (байты 0x04..0x07, little-endian) = `0x0802xxxx`.
- Автономный: `Patch skipped`; Reset-вектор = `0x0800xxxx`.

### Перенос механизма в другой проект (кратко)

1. В корень проекта: `patch_crc.py`, `patch_crc.bat`, `..._FLASH_BL.ld`
   (ORIGIN/LENGTH = адрес и размер образа из загрузчика; секция `.fw_header`
   сразу после `.isr_vector`; CCM-секции - только NOLOAD-вариант вне `.bss`,
   у F411 CCM отсутствует).
2. Паспорт `application_fw_header_t` и флаг в `main.c`/своём заголовке.
3. After build - строка выше; Script file - BL-скрипт (для режима загрузчика).
4. Проверка: образ из Release проходит `patch_crc.bat` и `check_application`
   загрузчика.
