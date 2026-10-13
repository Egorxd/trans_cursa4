param([switch]$Regenerate, [string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if ($Regenerate) {
        & bison --warnings=all --warnings=error --defines=generated/parser.h -o generated/parser.c grammar/parser.y
        if ($LASTEXITCODE -ne 0) { throw 'Bison failed' }
        & flex -o generated/lexer.c grammar/lexer.l
        if ($LASTEXITCODE -ne 0) { throw 'Flex failed' }
    }
    New-Item -ItemType Directory -Path bin -Force | Out-Null
    & $Compiler -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc -Igenerated -static -static-libgcc -municode src/main.c src/ast.c src/semantic.c src/codegen.c generated/parser.c generated/lexer.c -o bin/translator.exe
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
    & .\bin\translator.exe --version
    if ($LASTEXITCODE -ne 0) { throw 'Executable check failed' }
} finally { Pop-Location }
