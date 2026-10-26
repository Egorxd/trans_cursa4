param([string]$Python = '')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if (-not $Python) {
        if (Test-Path -LiteralPath '.venv\Scripts\python.exe') { $Python = '.\.venv\Scripts\python.exe' }
        else { $Python = 'python' }
    }
    & .\bin\translator.exe examples/input.txt -o examples/output.py
    if ($LASTEXITCODE -ne 0) { throw 'Translation failed' }
    & $Python tools/show_example.py
    if ($LASTEXITCODE -ne 0) { throw 'Python execution failed; install requirements.txt' }
} finally { Pop-Location }
