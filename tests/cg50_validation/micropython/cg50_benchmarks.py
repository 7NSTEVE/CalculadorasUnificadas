# cg50_benchmarks.py — 12 MicroPython Scientific & Graphics Benchmarks for Casio fx-CG50
# Compatible with Casio OS 3.80 MicroPython (math, random, casioplot).
# Run directly on Casio fx-CG50 MicroPython or verify deterministically on host Python 3.

import math


def bench_sieve(n=200):
    sieve = [True] * (n + 1)
    sieve[0] = sieve[1] = False
    p = 2
    while p * p <= n:
        if sieve[p]:
            for m in range(p * p, n + 1, p):
                sieve[m] = False
        p += 1
    primes = [i for i in range(n + 1) if sieve[i]]
    return len(primes), primes[-1]


def bench_newton_sqrt2(iters=12):
    x = 1.5
    for _ in range(iters):
        x = 0.5 * (x + 2.0 / x)
    return x


def bench_rk4_decay(steps=100, h=0.01):
    # dy/dt = -y, y(0) = 1 -> y(1) = e^-1
    y = 1.0
    for _ in range(steps):
        k1 = -y
        k2 = -(y + 0.5 * h * k1)
        k3 = -(y + 0.5 * h * k2)
        k4 = -(y + h * k3)
        y += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4)
    return y


def bench_gaussian_elim():
    # Solve 3x3 linear system: [2,1,-1; -3,-1,2; -2,1,2] * x = [8, -11, -3] -> x = [2, 3, -1]
    A = [[2.0, 1.0, -1.0, 8.0],
         [-3.0, -1.0, 2.0, -11.0],
         [-2.0, 1.0, 2.0, -3.0]]
    n = 3
    for i in range(n):
        piv = A[i][i]
        for j in range(i, n + 1):
            A[i][j] /= piv
        for k in range(i + 1, n):
            f = A[k][i]
            for j in range(i, n + 1):
                A[k][j] -= f * A[i][j]
    x = [0.0] * n
    for i in range(n - 1, -1, -1):
        s = A[i][n]
        for j in range(i + 1, n):
            s -= A[i][j] * x[j]
        x[i] = s
    return [round(v, 6) for v in x]


def bench_dft(N=16):
    # DFT of cos(2*pi*2*n/N) -> magnitude peak at k=2 equal to N/2 = 8.0
    sig = [math.cos(2.0 * math.pi * 2.0 * n / N) for n in range(N)]
    mags = []
    for k in range(N):
        re = 0.0
        im = 0.0
        for n in range(N):
            ang = 2.0 * math.pi * k * n / N
            re += sig[n] * math.cos(ang)
            im -= sig[n] * math.sin(ang)
        mags.append(math.sqrt(re * re + im * im))
    return round(mags[2], 6)


def bench_fft_radix2(x_re, x_im):
    N = len(x_re)
    if N <= 1:
        return x_re, x_im
    ev_r, ev_i = bench_fft_radix2(x_re[0::2], x_im[0::2])
    od_r, od_i = bench_fft_radix2(x_re[1::2], x_im[1::2])
    out_r = [0.0] * N
    out_i = [0.0] * N
    for k in range(N // 2):
        ang = -2.0 * math.pi * k / N
        wr, wi = math.cos(ang), math.sin(ang)
        tr = wr * od_r[k] - wi * od_i[k]
        ti = wr * od_i[k] + wi * od_r[k]
        out_r[k] = ev_r[k] + tr
        out_i[k] = ev_i[k] + ti
        out_r[k + N // 2] = ev_r[k] - tr
        out_i[k + N // 2] = ev_i[k] - ti
    return out_r, out_i


def bench_matmul(n=6):
    A = [[(i + 1) * (j + 2) for j in range(n)] for i in range(n)]
    B = [[(i + 2) + (j + 1) for j in range(n)] for i in range(n)]
    C = [[0] * n for _ in range(n)]
    for i in range(n):
        for j in range(n):
            s = 0
            for k in range(n):
                s += A[i][k] * B[k][j]
            C[i][j] = s
    return sum(C[i][i] for i in range(n))


def bench_quicksort(arr):
    if len(arr) <= 1:
        return arr
    p = arr[len(arr) // 2]
    lo = [x for x in arr if x < p]
    eq = [x for x in arr if x == p]
    hi = [x for x in arr if x > p]
    return bench_quicksort(lo) + eq + bench_quicksort(hi)


def bench_monte_carlo_pi(samples=1000):
    # Deterministic LCG PRNG for exact reproducibility across MicroPython & host
    state = 123456789
    inside = 0
    for _ in range(samples):
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        x = state / 2147483648.0
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        y = state / 2147483648.0
        if x * x + y * y <= 1.0:
            inside += 1
    return 4.0 * inside / samples


def bench_pendulum(steps=100, dt=0.01):
    # Nonlinear pendulum theta'' + (g/L)*sin(theta) = 0, g=9.81, L=1.0
    th = 0.5
    om = 0.0
    for _ in range(steps):
        a = -9.81 * math.sin(th)
        om += a * dt
        th += om * dt
    return round(th, 6)


def bench_rlc_step(steps=200, dt=1e-5):
    # Series RLC: R=100, L=0.01, C=1e-6, Vin=5V
    R, L, C, Vin = 100.0, 0.01, 1e-6, 5.0
    i_cur = 0.0
    vc = 0.0
    for _ in range(steps):
        di = (Vin - R * i_cur - vc) / L
        dvc = i_cur / C
        i_cur += di * dt
        vc += dvc * dt
    return round(vc, 6)


def bench_mandelbrot_casioplot(w=64, h=36, max_iter=16):
    # Uses casioplot.set_pixel when running on Casio fx-CG50, counts interior points everywhere
    try:
        import casioplot
        has_plot = True
    except ImportError:
        has_plot = False
    interior = 0
    for py in range(h):
        cy = -1.2 + 2.4 * py / h
        for px in range(w):
            cx = -2.0 + 2.8 * px / w
            zx = 0.0
            zy = 0.0
            it = 0
            while zx * zx + zy * zy <= 4.0 and it < max_iter:
                zx, zy = zx * zx - zy * zy + cx, 2.0 * zx * zy + cy
                it += 1
            if it == max_iter:
                interior += 1
                if has_plot:
                    casioplot.set_pixel(px * 4, py * 4, (0, 0, 128))
    if has_plot:
        casioplot.show_screen()
    return interior


def run_all():
    fft_r, fft_i = bench_fft_radix2([math.cos(2.0 * math.pi * 2.0 * n / 16) for n in range(16)], [0.0] * 16)
    res = {
        "sieve_200": bench_sieve(200),
        "newton_sqrt2": round(bench_newton_sqrt2(), 10),
        "rk4_exp_m1": round(bench_rk4_decay(), 8),
        "gauss_3x3": bench_gaussian_elim(),
        "dft_16_bin2": bench_dft(16),
        "fft_16_bin2": round(math.sqrt(fft_r[2] ** 2 + fft_i[2] ** 2), 6),
        "matmul_6x6_trace": bench_matmul(6),
        "quicksort_ok": bench_quicksort([9, 3, 7, 1, 8, 2, 6, 4, 5]) == [1, 2, 3, 4, 5, 6, 7, 8, 9],
        "monte_carlo_pi_1k": round(bench_monte_carlo_pi(1000), 4),
        "pendulum_1s": bench_pendulum(),
        "rlc_2ms_vc": bench_rlc_step(),
        "mandelbrot_interior": bench_mandelbrot_casioplot(),
    }
    return res


if __name__ == "__main__":
    out = run_all()
    for k, v in out.items():
        print(f"{k}: {v}")
