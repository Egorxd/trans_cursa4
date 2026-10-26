import ast
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'examples/input.txt'
OUTPUT = ROOT / 'examples/output.py'

def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8')
    lines = SOURCE.read_text(encoding='utf-8-sig').splitlines(keepends=True)
    tree = ast.parse(OUTPUT.read_text(encoding='utf-8'), filename=str(OUTPUT))
    definitions = {}
    inputs = []

    def statement(line, column):
        return (lines[line - 1][column - 1:] + ''.join(lines[line:])).split(';', 1)[0].strip()

    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign) or not isinstance(node.value, ast.Call):
            continue
        call = node.value
        if not isinstance(call.func, ast.Name) or call.func.id != '__finite':
            continue
        text = statement(call.args[-2].value, call.args[-1].value)
        match = re.fullmatch(r'([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.*)', text, re.S)
        if match:
            name, expression = match.groups()
            definitions[name] = expression.strip()
            if expression.lstrip().startswith('[['):
                inputs.append(f'{name} = {expression.strip()}')

    counter = 0
    descriptions = {
        'A+B': 'Сложение матриц',
        'A-B': 'Вычитание матриц',
        'A*B': 'Матричное умножение',
        'transpose(A)': 'Транспонирование — строки и столбцы меняются местами',
        'det(A)': 'Определитель матрицы A',
        'rank(A)': 'Ранг матрицы A',
        'inverse(A)': 'Обратная матрица для A',
    }

    def show(value, command):
        nonlocal counter
        counter += 1
        match = re.fullmatch(r'print\s*\((.*)\)', command, re.S)
        name = match.group(1).strip() if match else command
        expression = definitions.get(name, name)
        description = descriptions.get(re.sub(r'\s+', '', expression), 'Результат выражения')
        formula = f'{name} = {expression}' if name in definitions else expression
        print(f'\n{counter}. {description}')
        print(f'   {formula}')
        if hasattr(value, 'shape'):
            import numpy as np
            print(np.array2string(value, formatter={'float_kind': lambda number: f'{number:.10g}'}))
        else:
            print(f'{value:.10g}')

    class Captions(ast.NodeTransformer):
        def visit_Call(self, node):
            self.generic_visit(node)
            if not isinstance(node.func, ast.Name) or node.func.id != 'print' or len(node.args) != 1:
                return node
            value = node.args[0]
            if not isinstance(value, ast.Call) or not isinstance(value.func, ast.Name) or value.func.id != '__finite':
                return node
            text = statement(value.args[-2].value, value.args[-1].value)
            replacement = ast.Call(func=ast.Name(id='__demo_show', ctx=ast.Load()),
                                   args=[value, ast.Constant(value=text)], keywords=[])
            return ast.copy_location(replacement, node)

    print('\nДЕМОНСТРАЦИЯ МАТРИЧНЫХ ВЫЧИСЛЕНИЙ')
    print('\nИсходные матрицы:')
    print('\n'.join(inputs))
    print('\nНиже — результаты команд print из исходного файла.')
    print('Числа для удобства показаны с округлением до 10 значащих цифр.')
    translated = ast.fix_missing_locations(Captions().visit(tree))
    exec(compile(translated, str(OUTPUT), 'exec'), {'__name__': '__main__', '__demo_show': show})
    print('\nПример выполнен успешно.')

if __name__ == '__main__':
    main()
