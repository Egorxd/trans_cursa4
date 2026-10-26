import sys
for __stream in (sys.stdout, sys.stderr):
    if hasattr(__stream, 'reconfigure'):
        __stream.reconfigure(encoding='utf-8', errors='backslashreplace')
try:
    import numpy as np
except ImportError:
    print('Ошибка: установите NumPy: python -m pip install numpy==2.3.5', file=sys.stderr)
    sys.exit(3)

__source = "examples/input.txt"
np.set_printoptions(precision=10, suppress=False)
np.seterr(over='raise', invalid='raise', divide='raise')

class __MatrixError(Exception):
    pass

def __fail(message, line, column):
    raise __MatrixError(f'{__source}:{line}:{column}: ошибка вычисления: {message}')

def __rank(a, line, column):
    try:
        s = np.linalg.svd(a, compute_uv=False)
        if s[0] == 0.0:
            return 0
        return int(np.count_nonzero((s / s[0]) > max(a.shape) * np.finfo(np.float64).eps))
    except (np.linalg.LinAlgError, FloatingPointError) as exc:
        __fail(f'не удалось вычислить ранг: {exc}', line, column)

def __inverse(a, line, column):
    if __rank(a, line, column) < a.shape[0]:
        __fail('матрица вырождена или численно вырождена; обратная матрица не существует с выбранной точностью', line, column)
    try:
        return np.linalg.inv(a)
    except (np.linalg.LinAlgError, FloatingPointError) as exc:
        __fail(f'не удалось вычислить обратную матрицу: {exc}', line, column)

def __det(a, line, column):
    try:
        return float(np.linalg.det(a))
    except (np.linalg.LinAlgError, FloatingPointError) as exc:
        __fail(f'не удалось вычислить определитель: {exc}', line, column)

def __transpose(a, line, column):
    return a.T.copy()

def __finite(value, line, column):
    checked = value if isinstance(value, np.ndarray) else float(value)
    if not np.all(np.isfinite(checked)):
        __fail('результат вне диапазона float64', line, column)
    return value if isinstance(value, (np.ndarray, int)) and (isinstance(value, np.ndarray) or abs(value) <= 2**53) else checked

__line, __column = 1, 1
try:
    __line, __column = 1, 8
    __v0 = __finite(np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float64), 1, 8)
    __line, __column = 2, 8
    __v1 = __finite(np.array([[5.0, 6.0], [7.0, 8.0]], dtype=np.float64), 2, 8)
    __line, __column = 4, 8
    __v2 = __finite((__v0 + __v1), 4, 8)
    __line, __column = 5, 8
    __v3 = __finite((__v0 - __v1), 5, 8)
    __line, __column = 6, 8
    __v4 = __finite((__v0 @ __v1), 6, 8)
    __line, __column = 7, 8
    __v5 = __finite(__transpose(__v0, 7, 12), 7, 8)
    __line, __column = 8, 8
    __v6 = __finite(__det(__v0, 8, 12), 8, 8)
    __line, __column = 9, 8
    __v7 = __finite(__rank(__v0, 9, 12), 9, 8)
    __line, __column = 10, 8
    __v8 = __finite(__inverse(__v0, 10, 12), 10, 8)
    __line, __column = 12, 1
    print(__finite(__v2, 12, 1))
    __line, __column = 12, 11
    print(__finite(__v3, 12, 11))
    __line, __column = 12, 21
    print(__finite(__v4, 12, 21))
    __line, __column = 12, 31
    print(__finite(__v5, 12, 31))
    __line, __column = 13, 1
    print(__finite(__v6, 13, 1))
    __line, __column = 13, 11
    print(__finite(__v7, 13, 11))
    __line, __column = 13, 21
    print(__finite(__v8, 13, 21))
except __MatrixError as exc:
    print(str(exc), file=sys.stderr)
    sys.exit(1)
except (np.linalg.LinAlgError, FloatingPointError, OverflowError, ValueError, MemoryError) as exc:
    print(f'{__source}:{__line}:{__column}: ошибка вычисления: {exc}', file=sys.stderr)
    sys.exit(1)
