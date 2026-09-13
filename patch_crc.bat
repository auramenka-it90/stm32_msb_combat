@echo off
setlocal
set BIN_FILE=%~1
set SCRIPT_DIR=%~dp0

:: 1. Проверяем стандартный Windows Python Launcher (py)
py --version >nul 2>&1
if %errorlevel% equ 0 (
    py "%SCRIPT_DIR%patch_crc.py" "%BIN_FILE%"
    exit /b %errorlevel%
)

:: 2. Проверяем системный python в PATH
python --version >nul 2>&1
if %errorlevel% equ 0 (
    python "%SCRIPT_DIR%patch_crc.py" "%BIN_FILE%"
    exit /b %errorlevel%
)

:: 3. Проверяем локальные пути установки Python текущего пользователя
if exist "%LOCALAPPDATA%\Python\bin\python.exe" (
    "%LOCALAPPDATA%\Python\bin\python.exe" "%SCRIPT_DIR%patch_crc.py" "%BIN_FILE%"
    exit /b %errorlevel%
)

for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python3*") do (
    if exist "%%D\python.exe" (
        "%%D\python.exe" "%SCRIPT_DIR%patch_crc.py" "%BIN_FILE%"
        exit /b %errorlevel%
    )
)

:: 4. Проверяем системные папки (C:\Python3x)
for /d %%D in ("C:\Python3*") do (
    if exist "%%D\python.exe" (
        "%%D\python.exe" "%SCRIPT_DIR%patch_crc.py" "%BIN_FILE%"
        exit /b %errorlevel%
    )
)

echo [ERROR] Python not found on this PC! Install Python 3 to build with CRC.
exit /b 1