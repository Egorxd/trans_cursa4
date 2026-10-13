Matrix Translator 1.0.0

translator.exe: Windows 10/11 x64.
translator: Linux x86_64, статический ELF.

Сторонние DLL или .so для этих двух сборок не нужны.
Windows использует системные KERNEL32 и UCRT, входящие в Windows 10/11.
Python и NumPy нужны только для выполнения результата трансляции.

Справка:
  translator.exe --help
  ./translator --help

После извлечения архива в Linux при необходимости:
  chmod +x translator

Полный порядок сборки находится в BUILD.txt в родительской папке.
