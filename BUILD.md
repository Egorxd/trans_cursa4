# Сборка Matrix Translator

Выполняйте команды из корня проекта, где находится этот файл. Для сборки нужен компилятор C11. Flex и Bison нужны, если требуется заново создать анализаторы из файлов `grammar/lexer.l` и `grammar/parser.y`. Для выполнения примеров и тестов нужен Python 3.11 или новее с NumPy из `requirements.txt`.

## Windows x64

Установите MSYS2. Откройте терминал MSYS2 UCRT64 и установите GCC.

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Откройте PowerShell в корне проекта и соберите транслятор из файлов в `generated/`. Если MSYS2 установлен в другой каталог, поправьте путь к `gcc.exe`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Compiler 'C:\msys64\ucrt64\bin\gcc.exe'
.\bin\translator.exe --version
```

Результат сборки находится в `bin/translator.exe`. Команда проверки версии выводит `Matrix Translator 1.0.0`. Для работы этого файла достаточно системных библиотек Windows и UCRT.

### Сборка из грамматики

Установите Bison 3.8 или новее и Flex 2.6 или новее в терминале MSYS2, чтобы пересобрать анализаторы из файлов в `grammar/`.

```bash
pacman -S --needed bison flex
```

В PowerShell добавьте инструменты в PATH и запустите сборку.

```powershell
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;' + $env:PATH
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Regenerate
```

Скрипт обновит `generated/parser.c`, `generated/parser.h` и `generated/lexer.c`, затем соберёт `bin/translator.exe`. Чтобы внести изменения в анализаторы, редактируйте файлы в `grammar/`.

## Linux x86_64

Установите GCC и GNU Make. В Debian и Ubuntu они входят в пакет `build-essential`.

```bash
sudo apt-get update
sudo apt-get install build-essential
make -B
./bin/translator --version
```

Для сборки из грамматики установите генераторы и выполните `make grammar` перед компиляцией.

```bash
sudo apt-get install bison flex
make grammar
make -B
```

Обычная сборка использует системные библиотеки Linux. В архив включён статический `bin/translator`. Для статической сборки из исходников добавьте флаг `-static`.

```bash
make -B CFLAGS='-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -static'
```

## Проверка сборки

В Windows подготовьте окружение и запустите тесты.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\setup.ps1
.\.venv\Scripts\python.exe tests/run_tests.py
```

В Linux используйте Python 3.11 или новее с модулем `venv`.

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python tests/run_tests.py --translator bin/translator
```

При успешной проверке появится строка `93/93 passed`. Журналы предыдущих запусков находятся в [verification/](verification/). Для ручной проверки откройте раздел запуска в [справке пользователя](USER_GUIDE.md).

## Docker

Запустите Docker с поддержкой Linux-контейнеров. Сборка образа требует интернета. Dockerfile создаёт анализаторы из грамматики, компилирует транслятор и запускает тесты. В готовом образе есть транслятор, Python 3.12 и NumPy.

```sh
docker build -t matrix-translator:1.0.0 .
docker run --rm matrix-translator:1.0.0 --help
```

В PowerShell подключите папку проекта и выполните пример.

```powershell
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/work" matrix-translator:1.0.0 examples/input.txt -o demo_output.py
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/work" --entrypoint python matrix-translator:1.0.0 demo_output.py
```

В Linux используйте команды ниже.

```bash
docker run --rm --mount "type=bind,source=$PWD,target=/work" matrix-translator:1.0.0 examples/input.txt -o demo_output.py
docker run --rm --mount "type=bind,source=$PWD,target=/work" --entrypoint python matrix-translator:1.0.0 demo_output.py
```
