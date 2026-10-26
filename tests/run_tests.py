import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
POSITION = re.compile(r':\d+:\d+:')
NUMBER = re.compile(r'[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?')

def execute(arguments, *, env=None):
    return subprocess.run([str(a) for a in arguments], capture_output=True, text=True,
                          encoding='utf-8', errors='strict', timeout=30, env=env)

def check(condition, message):
    if not condition:
        raise AssertionError(message)

def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8', errors='backslashreplace')
    parser = argparse.ArgumentParser(description='Matrix Translator acceptance tests.')
    parser.add_argument('--translator', type=Path, default=ROOT/'bin'/('translator.exe' if os.name == 'nt' else 'translator'))
    parser.add_argument('--report', type=Path)
    options = parser.parse_args()
    translator = options.translator.resolve()
    cases = json.loads((ROOT/'tests/cases.json').read_text(encoding='utf-8'))
    results = []

    def record(name, action):
        try:
            data = action() or {}
            results.append(dict(name=name, passed=True, **data))
            print(f'PASS {name}')
        except Exception as exc:
            results.append(dict(name=name, passed=False, error=str(exc)))
            print(f'FAIL {name}: {exc}')

    with tempfile.TemporaryDirectory(prefix='matrix-translator-') as temporary:
        folder = Path(temporary)
        for case in cases:
            def run_case(case=case):
                source = ROOT/case['path']
                output = folder/(case['name']+'.py')
                compiled = execute([translator, source, '-o', output])
                if case['kind'] == 'invalid':
                    check(compiled.returncode == 1, f'expected code 1, got {compiled.returncode}: {compiled.stderr}')
                    check(case['error'] in compiled.stderr, f'wrong diagnostic: {compiled.stderr}')
                    check(POSITION.search(compiled.stderr), 'missing source position')
                    check(not output.exists(), 'an output file was created for invalid input')
                    output.write_bytes(b'preserve this existing file\n')
                    repeated = execute([translator, source, '-o', output])
                    check(repeated.returncode == 1, 'wrong exit status with existing output')
                    check(output.read_bytes() == b'preserve this existing file\n', 'existing output was changed on error')
                    return dict(description=case['description'], diagnostic=compiled.stderr.strip())
                check(compiled.returncode == 0, compiled.stderr)
                check(output.is_file(), 'missing Python output')
                if case['name'] == 'all_operations':
                    check(' @ ' in output.read_text(encoding='utf-8'), 'matrix * was not translated into @')
                executed = execute([sys.executable, output])
                if 'runtime_error' in case:
                    check(executed.returncode == 1, f'wrong runtime status: {executed.returncode}')
                    check(case['runtime_error'] in executed.stderr, executed.stderr)
                    check('Traceback' not in executed.stderr, 'raw Python traceback')
                    check(POSITION.search(executed.stderr), 'missing runtime source position')
                else:
                    check(executed.returncode == 0, executed.stderr)
                    values = [float(value) for value in NUMBER.findall(executed.stdout)]
                    check(len(values) == len(case['expected']), f'wrong number of values: {values}')
                    for actual, expected in zip(values, case['expected']):
                        check(math.isclose(actual, expected, rel_tol=1e-9, abs_tol=1e-10), f'{actual} != {expected}')
                return dict(description=case['description'], stdout=executed.stdout, stderr=executed.stderr)
            record(case['name'], run_case)

        def cli(name, args, status, fragment):
            def action():
                result = execute([translator, *args])
                check(result.returncode == status, f'{result.returncode}: {result.stderr}')
                check(fragment in result.stdout+result.stderr, 'missing usage/diagnostic')
                return dict(stdout=result.stdout, stderr=result.stderr)
            record(name, action)

        source = folder/'input.txt'
        source.write_text('print(42);', encoding='utf-8')
        cli('cli_no_arguments', [], 2, 'translator input.txt -o output.py')
        cli('cli_missing_output', [source], 2, 'Использование')
        cli('cli_wrong_switch', [source, '--output', folder/'bad.py'], 2, 'Использование')
        cli('cli_unknown_option', ['--unknown'], 2, 'Использование')
        cli('cli_help', ['--help'], 0, 'Использование')
        cli('cli_version', ['--version'], 0, '1.0.0')
        cli('cli_missing_file', [folder/'missing.txt', '-o', folder/'missing.py'], 3, 'входной файл')
        cli('cli_read_directory', [folder, '-o', folder/'directory.py'], 3, 'входной файл')
        cli('cli_missing_output_directory', [source, '-o', folder/'absent'/'out.py'], 3, 'выходной файл')
        cli('cli_output_is_directory', [source, '-o', folder], 3, 'выходной файл')
        cli('cli_same_file', [source, '-o', source], 3, 'должны быть разными')
        check(source.read_text(encoding='utf-8') == 'print(42);', 'input was modified')

        def hardlink():
            target = folder/'input-alias.txt'
            os.link(source, target)
            result = execute([translator, source, '-o', target])
            check(result.returncode == 3, 'hardlink not detected')
            check(source.read_text(encoding='utf-8') == 'print(42);', 'input hardlink modified')
        record('same_file_hardlink', hardlink)

        def utf8_paths():
            subfolder = folder/'папка с пробелами'
            subfolder.mkdir()
            name = "исходный 'текст'.txt"
            input_path = subfolder/name
            input_path.write_bytes(b'\xef\xbb\xbf'+ '// Русский комментарий\r\nprint(rank([[1]]));\r\n'.encode('utf-8'))
            output_path = subfolder/'готовая программа.py'
            result = execute([translator, input_path, '-o', output_path])
            check(result.returncode == 0, result.stderr)
            result = execute([sys.executable, output_path])
            check(result.returncode == 0 and result.stdout.strip() == '1', result.stderr+result.stdout)
        record('utf8_bom_crlf_paths', utf8_paths)

        def unicode_column():
            input_path = folder/'column.txt'
            input_path.write_text('/* привет */\tprint(A);', encoding='utf-8')
            result = execute([translator, input_path, '-o', folder/'column.py'])
            check(result.returncode == 1 and ':1:20:' in result.stderr, result.stderr)
            check(not (folder/'column.py').exists(), 'invalid input created a file')
            return dict(stderr=result.stderr)
        record('unicode_comment_error_column', unicode_column)

        def runtime_error_legacy_encoding():
            input_path = folder/'legacy-encoding.txt'
            output_path = folder/'legacy-encoding.py'
            input_path.write_text('print(inverse([[0]]));', encoding='utf-8')
            translated = execute([translator, input_path, '-o', output_path])
            check(translated.returncode == 0, translated.stderr)
            environment = dict(os.environ, PYTHONIOENCODING='cp1251', PYTHONUTF8='0')
            result = execute([sys.executable, output_path], env=environment)
            check(result.returncode == 1, f'wrong runtime status: {result.returncode}')
            check('матрица вырождена' in result.stderr, result.stderr)
            check('ошибка вычисления' in result.stderr, result.stderr)
            check(POSITION.search(result.stderr), 'missing source position')
            check('Traceback' not in result.stderr, 'raw Python traceback')
            return dict(stderr=result.stderr)
        record('runtime_error_utf8_with_cp1251_environment', runtime_error_legacy_encoding)

        def replace_output():
            output = folder/'overwrite.py'
            output.write_text('old contents', encoding='utf-8')
            result = execute([translator, source, '-o', output])
            check(result.returncode == 0, result.stderr)
            result = execute([sys.executable, output])
            check(result.returncode == 0 and result.stdout.strip() == '42.0', result.stderr+result.stdout)
        record('successful_output_replacement', replace_output)

        def limit():
            large = folder/'large.txt'
            with large.open('wb') as file:
                file.write(b' '*(16*1024*1024+1))
            result = execute([translator, large, '-o', folder/'large.py'])
            check(result.returncode == 3 and '16 МиБ' in result.stderr, result.stderr)
            check(not (folder/'large.py').exists(), 'large file produced output')
        record('input_size_limit', limit)

        def token_limit():
            token_file = folder/'tokens.txt'
            token_file.write_text('print(1);\n'*21000, encoding='utf-8')
            result = execute([translator, token_file, '-o', folder/'tokens.py'])
            check(result.returncode == 1 and '100000 токенов' in result.stderr, result.stderr)
            check(not (folder/'tokens.py').exists(), 'token limit produced output')
        record('token_count_limit', token_limit)

        def inverse_residual():
            matrix_source = folder/'residual.txt'
            matrix_source.write_text('matrix A=[[4,1,2],[1,3,0],[2,0,5]]; print(A*inverse(A));', encoding='utf-8')
            output = folder/'residual.py'
            result = execute([translator, matrix_source, '-o', output])
            check(result.returncode == 0, result.stderr)
            result = execute([sys.executable, output])
            check(result.returncode == 0, result.stderr)
            actual = [float(x) for x in NUMBER.findall(result.stdout)]
            check(len(actual) == 9, result.stdout)
            expected = [1,0,0,0,1,0,0,0,1]
            check(max(abs(a-b) for a,b in zip(actual,expected)) < 1e-9, 'inverse residual exceeds tolerance')
            return dict(stdout=result.stdout)
        record('inverse_residual', inverse_residual)
        record('temporary_files_cleaned', lambda: check(not list(folder.rglob('*.tmp.*')), 'temporary output files leaked'))

    passed = sum(result['passed'] for result in results)
    report = dict(total=len(results), passed=passed, failed=len(results)-passed,
                  platform=sys.platform, python=sys.version, results=results)
    if options.report:
        options.report.parent.mkdir(parents=True, exist_ok=True)
        options.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f'\n{passed}/{len(results)} passed')
    return 0 if passed == len(results) else 1

if __name__ == '__main__':
    raise SystemExit(main())
