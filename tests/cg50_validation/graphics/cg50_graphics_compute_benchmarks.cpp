// cg50_graphics_compute_benchmarks.cpp — Phase 5 Intensive Graphics & Scientific Compute Benchmarks
//
// Executes 11 authentic Casio MicroPython (casioplot + math) workloads directly inside
// REAL Casio fx-CG50 OS 3.80 firmware on Cg50NativeCore (SH7305, zero HLE, strict_no_fpu=true):
//   1. GFX-MANDELBROT    — Mandelbrot set smooth RGB palette
//   2. GFX-JULIA         — Julia set z^2 + (-0.70176 - 0.3842i)
//   3. GFX-NEWTON-Z3     — Newton fractal for z^3 - 1 = 0 (3-basin convergence)
//   4. GFX-SIERPINSKI    — Sierpinski Triangle Chaos Game (12,000 pts)
//   5. GFX-LORENZ-RK4    — Lorenz Strange Attractor via 4th-order Runge-Kutta
//   6. GFX-TORUS-3D      — 3D Torus Knot (p=3, q=5) perspective projection with Z-shading
//   7. GFX-NBODY         — 3-Body Gravitational Figure-Eight Orbit (Velocity Verlet)
//   8. GFX-RLC-TRANSIENT — Underdamped RLC transient + exponential envelope
//   9. COMP-FFT-DFT      — 32-point DFT two-tone spectrum analyzer + visual spectrum bars
//  10. COMP-RK4-ODE      — RK4 ODE solver vs analytic exact solution + error plot
//  11. COMP-ULTRA-MATH   — Dense 10x10 LU + Chaotic Logistic n=10,000 + 3D Triple Integral GL6
//
// Each script is injected into a thread-private copy of UserROM via Cg50SmemFs (Fugue FTL + FAT16),
// executed by the real Casio OS 3.80 MicroPython app (ALPHA+S<->D -> F1 RUN), and captured directly
// from the R61524 384x216 RGB565 LCD framebuffer.

#include "cg50_native_core.h"
#include "cg50_core/cg50_smem_fs.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kW = 384;
constexpr int kH = 216;
constexpr int kPixels = kW * kH;
constexpr std::uint64_t kChunkBudget = 2000000ull;

constexpr std::uint16_t kF1    = 0x070A;
constexpr std::uint16_t kMenu  = 0x0409;
constexpr std::uint16_t kAlpha = 0x0708;
constexpr std::uint16_t kSD    = 0x0606; // 'H' -> Python app on CG50 Main Menu
constexpr std::uint16_t kDown  = 0x0308;
constexpr std::uint16_t kExe   = 0x0302;
constexpr std::uint16_t kExit  = 0x0408;

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& out)
{
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto sz = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    out.resize(sz);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz)));
}

std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += static_cast<char>(c);
    }
    return out;
}

std::uint64_t Fnv1a32(const std::uint32_t* p, std::size_t n)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= static_cast<std::uint64_t>(p[i] & 0xFFFFFFu);
        h *= 0x00000100000001b3ULL;
    }
    return h;
}

bool WriteBmp24(const fs::path& path, const std::uint32_t* disp)
{
    fs::create_directories(path.parent_path());
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;
    constexpr std::uint32_t rowBytes = kW * 3;
    constexpr std::uint32_t imgBytes = rowBytes * kH;
    std::uint8_t hdr[54] = {};
    hdr[0] = 'B'; hdr[1] = 'M';
    const std::uint32_t fileSize = 54u + imgBytes;
    std::memcpy(hdr + 2, &fileSize, 4);
    const std::uint32_t offBits = 54u;
    std::memcpy(hdr + 10, &offBits, 4);
    const std::uint32_t dibSize = 40u;
    std::memcpy(hdr + 14, &dibSize, 4);
    const std::int32_t w = kW;
    const std::int32_t h = -kH; // top-down
    std::memcpy(hdr + 18, &w, 4);
    std::memcpy(hdr + 22, &h, 4);
    const std::uint16_t planes = 1, bpp = 24;
    std::memcpy(hdr + 26, &planes, 2);
    std::memcpy(hdr + 28, &bpp, 2);
    std::memcpy(hdr + 34, &imgBytes, 4);
    std::fwrite(hdr, 1, 54, f);
    std::vector<std::uint8_t> row(rowBytes);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const std::uint32_t argb = disp[y * kW + x];
            row[x * 3 + 0] = static_cast<std::uint8_t>(argb & 0xFFu);
            row[x * 3 + 1] = static_cast<std::uint8_t>((argb >> 8) & 0xFFu);
            row[x * 3 + 2] = static_cast<std::uint8_t>((argb >> 16) & 0xFFu);
        }
        std::fwrite(row.data(), 1, rowBytes, f);
    }
    std::fclose(f);
    return true;
}

struct BenchSpec {
    std::string id;
    std::string category;
    std::string title;
    std::string script;
    int maxComputeChunks;
    int minColoredPixels;
    int minUniqueColors;
};

struct BenchResult {
    std::string id;
    std::string category;
    std::string title;
    bool passed = false;
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;
    std::uint64_t bcdOps = 0;
    std::uint64_t fpuOps = 0;
    std::uint64_t dspOps = 0;
    double elapsedMs = 0.0;
    double mips = 0.0;
    int coloredPixels = 0;
    int darkPixels = 0;
    int uniqueColors = 0;
    std::uint64_t fbHash = 0;
    std::string bmpPath;
    std::string notes;
};

std::vector<BenchSpec> BuildBenchmarks()
{
    return {
        {
            "GFX-MANDELBROT",
            "fractal_2d",
            "Mandelbrot Set (96x96 -> 2x2 full RGB palette)",
            "from casioplot import *\n"
            "clear_screen()\n"
            "for py in range(96):\n"
            "  ci = -1.2 + py * 2.4 / 95.0\n"
            "  y2 = 16 + py * 2\n"
            "  for px in range(128):\n"
            "    cr = -2.1 + px * 2.9 / 127.0\n"
            "    zr = 0.0\n"
            "    zi = 0.0\n"
            "    it = 0\n"
            "    while zr*zr + zi*zi <= 4.0 and it < 28:\n"
            "      zr, zi = zr*zr - zi*zi + cr, 2.0*zr*zi + ci\n"
            "      it += 1\n"
            "    if it == 28:\n"
            "      col = (10, 14, 28)\n"
            "    else:\n"
            "      col = ((it*19)%256, (it*9+40)%256, (255-it*8)%256)\n"
            "    x2 = 64 + px * 2\n"
            "    set_pixel(x2, y2, col)\n"
            "    set_pixel(x2+1, y2, col)\n"
            "    set_pixel(x2, y2+1, col)\n"
            "    set_pixel(x2+1, y2+1, col)\n"
            "draw_string(4, 2, 'MANDELBROT 128x96x28', (20,20,140), 'small')\n"
            "show_screen()\n",
            2200, 12000, 12
        },
        {
            "GFX-JULIA",
            "fractal_2d",
            "Julia Set c = -0.70176 - 0.3842i (RGB palette)",
            "from casioplot import *\n"
            "clear_screen()\n"
            "cr = -0.70176\n"
            "ci = -0.3842\n"
            "for py in range(92):\n"
            "  y0 = -1.15 + py * 2.3 / 91.0\n"
            "  y2 = 20 + py * 2\n"
            "  for px in range(128):\n"
            "    x0 = -1.6 + px * 3.2 / 127.0\n"
            "    zr = x0\n"
            "    zi = y0\n"
            "    it = 0\n"
            "    while zr*zr + zi*zi <= 4.0 and it < 30:\n"
            "      zr, zi = zr*zr - zi*zi + cr, 2.0*zr*zi + ci\n"
            "      it += 1\n"
            "    if it == 30:\n"
            "      col = (15, 5, 35)\n"
            "    else:\n"
            "      col = ((it*23+30)%256, (it*13)%256, (240-it*7)%256)\n"
            "    x2 = 64 + px * 2\n"
            "    set_pixel(x2, y2, col)\n"
            "    set_pixel(x2+1, y2, col)\n"
            "    set_pixel(x2, y2+1, col)\n"
            "    set_pixel(x2+1, y2+1, col)\n"
            "draw_string(4, 2, 'JULIA c=-0.702-0.384i', (120,20,90), 'small')\n"
            "show_screen()\n",
            2400, 12000, 12
        },
        {
            "GFX-NEWTON-Z3",
            "fractal_2d",
            "Newton Fractal z^3 - 1 = 0 (3-Basin RGB Convergence)",
            "from casioplot import *\n"
            "clear_screen()\n"
            "for py in range(90):\n"
            "  y0 = -1.5 + py * 3.0 / 89.0\n"
            "  y2 = 20 + py * 2\n"
            "  for px in range(120):\n"
            "    x0 = -2.0 + px * 4.0 / 119.0\n"
            "    zr = x0\n"
            "    zi = y0\n"
            "    if zr == 0.0 and zi == 0.0:\n"
            "      zr = 0.001\n"
            "    it = 0\n"
            "    while it < 16:\n"
            "      r2 = zr*zr - zi*zi\n"
            "      i2 = 2.0*zr*zi\n"
            "      den = 3.0*(r2*r2 + i2*i2)\n"
            "      if den < 1e-9:\n"
            "        break\n"
            "      numr = r2*zr - i2*zi - 1.0\n"
            "      numi = r2*zi + i2*zr\n"
            "      dr = (numr*r2 + numi*i2)/den\n"
            "      di = (numi*r2 - numr*i2)/den\n"
            "      zr -= dr\n"
            "      zi -= di\n"
            "      if dr*dr + di*di < 1e-4:\n"
            "        break\n"
            "      it += 1\n"
            "    sh = max(60, 255 - it*13)\n"
            "    if zr > 0.2:\n"
            "      col = (sh, 40, 40)\n"
            "    elif zi > 0.0:\n"
            "      col = (40, sh, 60)\n"
            "    else:\n"
            "      col = (40, 70, sh)\n"
            "    x2 = 72 + px * 2\n"
            "    set_pixel(x2, y2, col)\n"
            "    set_pixel(x2+1, y2, col)\n"
            "    set_pixel(x2, y2+1, col)\n"
            "    set_pixel(x2+1, y2+1, col)\n"
            "draw_string(4, 2, 'NEWTON FRACTAL z^3-1=0', (20,80,20), 'small')\n"
            "show_screen()\n",
            2600, 12000, 10
        },
        {
            "GFX-SIERPINSKI",
            "fractal_2d",
            "Sierpinski Triangle Chaos Game (12,000 iterations)",
            "from casioplot import *\n"
            "clear_screen()\n"
            "vx = (192, 42, 342)\n"
            "vy = (22, 198, 198)\n"
            "cols = ((220,30,50), (20,140,60), (30,70,220))\n"
            "x = 192.0\n"
            "y = 110.0\n"
            "s = 1234567\n"
            "for i in range(12000):\n"
            "  s = (s * 1103515245 + 12345) & 0x7FFFFFFF\n"
            "  k = (s >> 16) % 3\n"
            "  x = 0.5 * (x + vx[k])\n"
            "  y = 0.5 * (y + vy[k])\n"
            "  if i > 20:\n"
            "    set_pixel(int(x), int(y), cols[k])\n"
            "draw_string(4, 2, 'SIERPINSKI 12000 PTS', (30,30,30), 'small')\n"
            "show_screen()\n",
            1200, 4000, 4
        },
        {
            "GFX-LORENZ-RK4",
            "dynamical_system",
            "Lorenz Strange Attractor (RK4, 1500 steps)",
            "from casioplot import *\n"
            "clear_screen()\n"
            "sig = 10.0\n"
            "rho = 28.0\n"
            "bet = 8.0 / 3.0\n"
            "x, y, z = 1.0, 1.0, 1.0\n"
            "dt = 0.008\n"
            "def f(a, b, c):\n"
            "  return (sig*(b-a), a*(rho-c)-b, a*b-bet*c)\n"
            "for i in range(1500):\n"
            "  k1 = f(x, y, z)\n"
            "  k2 = f(x+0.5*dt*k1[0], y+0.5*dt*k1[1], z+0.5*dt*k1[2])\n"
            "  k3 = f(x+0.5*dt*k2[0], y+0.5*dt*k2[1], z+0.5*dt*k2[2])\n"
            "  k4 = f(x+dt*k3[0], y+dt*k3[1], z+dt*k3[2])\n"
            "  x += (dt/6.0)*(k1[0]+2*k2[0]+2*k3[0]+k4[0])\n"
            "  y += (dt/6.0)*(k1[1]+2*k2[1]+2*k3[1]+k4[1])\n"
            "  z += (dt/6.0)*(k1[2]+2*k2[2]+2*k3[2]+k4[2])\n"
            "  px = int(192 + x * 6.5)\n"
            "  py = int(196 - z * 3.4)\n"
            "  if 0 <= px < 383 and 16 <= py < 205:\n"
            "    c = (min(255, int(i*255/1500)), 40, max(0, 255-int(i*255/1500)))\n"
            "    set_pixel(px, py, c)\n"
            "    set_pixel(px+1, py, c)\n"
            "draw_string(4, 2, 'LORENZ ATTRACTOR RK4', (180,30,40), 'small')\n"
            "show_screen()\n",
            1400, 1200, 10
        },
        {
            "GFX-TORUS-3D",
            "graphics_3d",
            "3D Torus Knot (p=3, q=5) Perspective + Z-Shading",
            "from casioplot import *\n"
            "from math import sin, cos, pi\n"
            "clear_screen()\n"
            "N = 1400\n"
            "for i in range(N):\n"
            "  t = 2.0 * pi * i / N\n"
            "  r = cos(5.0 * t) + 2.2\n"
            "  x = r * cos(3.0 * t)\n"
            "  y = r * sin(3.0 * t)\n"
            "  z = -sin(5.0 * t)\n"
            "  yr = y * 0.82 - z * 0.57\n"
            "  zr = y * 0.57 + z * 0.82\n"
            "  sc = 210.0 / (zr + 6.5)\n"
            "  px = int(192 + x * sc)\n"
            "  py = int(112 - yr * sc)\n"
            "  sh = max(30, min(250, int((zr + 3.5) * 35)))\n"
            "  col = (sh, 90, 255 - sh)\n"
            "  for dx in (-1, 0, 1):\n"
            "    for dy in (-1, 0, 1):\n"
            "      if 0 <= px+dx < 384 and 16 <= py+dy < 208:\n"
            "        set_pixel(px+dx, py+dy, col)\n"
            "draw_string(4, 2, '3D TORUS KNOT (3,5)', (20,70,180), 'small')\n"
            "show_screen()\n",
            1200, 3000, 10
        },
        {
            "GFX-NBODY",
            "physics_sim",
            "3-Body Gravitational Figure-Eight Orbit (Velocity Verlet)",
            "from casioplot import *\n"
            "from math import sqrt\n"
            "clear_screen()\n"
            "x = [0.97000436, -0.97000436, 0.0]\n"
            "y = [-0.24308753, 0.24308753, 0.0]\n"
            "vx = [0.466203685, 0.466203685, -0.93240737]\n"
            "vy = [0.43236573, 0.43236573, -0.86473146]\n"
            "cols = ((220,30,30), (20,150,40), (30,60,220))\n"
            "dt = 0.004\n"
            "def acc(px, py):\n"
            "  ax = [0.0, 0.0, 0.0]\n"
            "  ay = [0.0, 0.0, 0.0]\n"
            "  for i in range(3):\n"
            "    for j in range(3):\n"
            "      if i != j:\n"
            "        dx = px[j] - px[i]\n"
            "        dy = py[j] - py[i]\n"
            "        d3 = (dx*dx + dy*dy + 1e-6) ** 1.5\n"
            "        ax[i] += dx / d3\n"
            "        ay[i] += dy / d3\n"
            "  return ax, ay\n"
            "ax, ay = acc(x, y)\n"
            "for st in range(900):\n"
            "  for i in range(3):\n"
            "    x[i] += vx[i]*dt + 0.5*ax[i]*dt*dt\n"
            "    y[i] += vy[i]*dt + 0.5*ay[i]*dt*dt\n"
            "  nx, ny = acc(x, y)\n"
            "  for i in range(3):\n"
            "    vx[i] += 0.5*(ax[i]+nx[i])*dt\n"
            "    vy[i] += 0.5*(ay[i]+ny[i])*dt\n"
            "    px = int(192 + x[i]*130)\n"
            "    py = int(112 - y[i]*130)\n"
            "    if 0 <= px < 383 and 16 <= py < 205:\n"
            "      set_pixel(px, py, cols[i])\n"
            "      set_pixel(px+1, py, cols[i])\n"
            "  ax, ay = nx, ny\n"
            "draw_string(4, 2, '3-BODY FIGURE-8 ORBIT', (20,40,120), 'small')\n"
            "show_screen()\n",
            1400, 1200, 4
        },
        {
            "GFX-RLC-TRANSIENT",
            "electrical_eng",
            "Underdamped RLC Step Response + Envelope",
            "from casioplot import *\n"
            "from math import exp, sin, cos, sqrt\n"
            "clear_screen()\n"
            "for gx in range(20, 370):\n"
            "  set_pixel(gx, 115, (180,180,180))\n"
            "for gy in range(24, 200):\n"
            "  set_pixel(20, gy, (180,180,180))\n"
            "R = 20.0; L = 0.01; C = 1e-6\n"
            "al = R / (2.0 * L)\n"
            "w0 = 1.0 / sqrt(L * C)\n"
            "wd = sqrt(w0*w0 - al*al)\n"
            "for px in range(350):\n"
            "  t = px * 0.004 / 349.0\n"
            "  e = exp(-al * t)\n"
            "  v = 1.0 - e * (cos(wd*t) + (al/wd)*sin(wd*t))\n"
            "  y_v = int(180 - v * 75)\n"
            "  y_hi = int(180 - (1.0 + e) * 75)\n"
            "  y_lo = int(180 - (1.0 - e) * 75)\n"
            "  if 20 <= y_v < 204:\n"
            "    set_pixel(20+px, y_v, (20,70,220))\n"
            "    set_pixel(20+px, y_v+1, (20,70,220))\n"
            "  if (px % 3) == 0:\n"
            "    if 20 <= y_hi < 204: set_pixel(20+px, y_hi, (210,50,50))\n"
            "    if 20 <= y_lo < 204: set_pixel(20+px, y_lo, (210,50,50))\n"
            "draw_string(4, 2, 'RLC STEP R=20 L=10mH C=1uF', (20,50,150), 'small')\n"
            "show_screen()\n",
            800, 600, 4
        },
        {
            "COMP-FFT-DFT",
            "dsp_spectral",
            "32-Point DFT Two-Tone Spectrum (f1=4, f2=11)",
            "from casioplot import *\n"
            "from math import sin, cos, pi, sqrt\n"
            "clear_screen()\n"
            "N = 32\n"
            "sig = [1.0*sin(2*pi*4*n/N) + 0.6*cos(2*pi*11*n/N) for n in range(N)]\n"
            "mags = []\n"
            "for k in range(N // 2):\n"
            "  re = 0.0; im = 0.0\n"
            "  for n in range(N):\n"
            "    ang = 2.0 * pi * k * n / N\n"
            "    re += sig[n] * cos(ang)\n"
            "    im -= sig[n] * sin(ang)\n"
            "  mags.append(sqrt(re*re + im*im) * 2.0 / N)\n"
            "for k in range(16):\n"
            "  h = int(mags[k] * 130)\n"
            "  x0 = 24 + k * 21\n"
            "  col = (220,40,40) if mags[k] > 0.4 else (50,120,210)\n"
            "  for dx in range(14):\n"
            "    for dy in range(max(2, h)):\n"
            "      set_pixel(x0+dx, 195-dy, col)\n"
            "draw_string(4, 2, 'DFT32 k4=%.2f k11=%.2f'%(mags[4],mags[11]), (20,20,20), 'small')\n"
            "show_screen()\n",
            800, 2000, 3
        },
        {
            "COMP-RK4-ODE",
            "numerical_ode",
            "RK4 ODE y'=-y+sin(x) vs Analytic Exact Solution",
            "from casioplot import *\n"
            "from math import sin, cos, exp\n"
            "clear_screen()\n"
            "x = 0.0; y = 1.0; h = 0.05; maxe = 0.0\n"
            "for i in range(100):\n"
            "  k1 = -y + sin(x)\n"
            "  k2 = -(y + 0.5*h*k1) + sin(x + 0.5*h)\n"
            "  k3 = -(y + 0.5*h*k2) + sin(x + 0.5*h)\n"
            "  k4 = -(y + h*k3) + sin(x + h)\n"
            "  y += (h/6.0)*(k1 + 2*k2 + 2*k3 + k4)\n"
            "  x += h\n"
            "  ex = 0.5*(sin(x) - cos(x) + 3.0*exp(-x))\n"
            "  err = abs(y - ex)\n"
            "  if err > maxe: maxe = err\n"
            "  px = 20 + i * 3\n"
            "  py = int(125 - y * 65)\n"
            "  if 20 <= py < 200:\n"
            "    set_pixel(px, py, (20,130,50))\n"
            "    set_pixel(px+1, py, (20,130,50))\n"
            "    set_pixel(px, py+1, (20,130,50))\n"
            "draw_string(4, 2, 'RK4 ODE maxErr=%.2e'%maxe, (20,90,40), 'small')\n"
            "draw_string(4, 18, 'y(5)=%.6f'%y, (30,30,30), 'small')\n"
            "show_screen()\n",
            800, 350, 3
        },
        {
            "COMP-ULTRA-MATH",
            "ultra_compute_v46",
            "Ultra Compute: 8x8 Dense LU + Logistic n=10000 + 3D Triple Integral GL6",
            "from casioplot import *\n"
            "from math import exp\n"
            "clear_screen()\n"
            "# 1. Logistic chaotic recurrence n=10000\n"
            "lx = 0.123456789\n"
            "for _ in range(10000):\n"
            "  lx = 4.0 * lx * (1.0 - lx)\n"
            "# 2. 3D Triple Integral GL6 on [0,1]^3: exp(xyz)/(1+x^2+y^2+z^2)\n"
            "nd = (0.0337652429,0.1693953068,0.3806904070,0.6193095930,0.8306046932,0.9662347571)\n"
            "wt = (0.0856622462,0.1803807865,0.2339569673,0.2339569673,0.1803807865,0.0856622462)\n"
            "tri = 0.0\n"
            "for i in range(6):\n"
            "  x = nd[i]; wx = wt[i]\n"
            "  for j in range(6):\n"
            "    y = nd[j]; wxy = wx * wt[j]; xy = x*y; d2 = 1.0 + x*x + y*y\n"
            "    for k in range(6):\n"
            "      z = nd[k]\n"
            "      tri += wxy * wt[k] * exp(xy * z) / (d2 + z*z)\n"
            "# 3. Dense 8x8 diagonally dominant LU solve\n"
            "n = 8\n"
            "A = [[(10.0 if i==j else 1.0/(i+j+1.0)) for j in range(n)] for i in range(n)]\n"
            "b = [sum(A[i]) for i in range(n)]\n"
            "for k in range(n):\n"
            "  for i in range(k+1, n):\n"
            "    f = A[i][k] / A[k][k]\n"
            "    for j in range(k, n): A[i][j] -= f * A[k][j]\n"
            "    b[i] -= f * b[k]\n"
            "xs = [0.0]*n\n"
            "for i in range(n-1, -1, -1):\n"
            "  xs[i] = (b[i] - sum(A[i][j]*xs[j] for j in range(i+1, n))) / A[i][i]\n"
            "lu_err = max(abs(v - 1.0) for v in xs)\n"
            "draw_string(6, 12, 'ULTRA COMPUTE SUITE v46', (20,40,160), 'medium')\n"
            "draw_string(6, 46, 'LOGISTIC 10K: %.8f'%lx, (180,30,30), 'medium')\n"
            "draw_string(6, 82, 'TRIPLE INT:   %.8f'%tri, (20,120,40), 'medium')\n"
            "draw_string(6, 118, '8x8 LU ERR:   %.2e'%lu_err, (30,60,180), 'medium')\n"
            "draw_string(6, 154, 'EXACT 3D REF: 0.59591828', (60,60,60), 'small')\n"
            "show_screen()\n",
            2200, 500, 3
        }
    };
}

class PyBenchSession {
public:
    bool LoadColdIntoPythonList(const std::vector<std::uint8_t>& sys,
                                const std::vector<std::uint8_t>& userWithAllPy,
                                const std::vector<std::uint8_t>& c2)
    {
        core_ = std::make_unique<Cg50NativeCore>();
        if (!core_->LoadFirmwareParts(sys.data(), sys.size(), userWithAllPy.data(), userWithAllPy.size())) return false;
        if (!core_->LoadC2Rom(c2.data(), c2.size())) return false;
        core_->SetStrictSh7305NoFpu(true);

        // Cold boot from reset until Casio OS 3.80 Main Menu reaches SLEEP (~198M inst)
        for (int i = 0; i < 360; ++i) {
            if (core_->GetState() == Cg50NativeCore::State::Running) {
                core_->RunInstructions(1000000ull);
            } else if (core_->GetState() == Cg50NativeCore::State::Sleeping) {
                core_->AdvanceCycles(1000000ull);
                if (core_->GetStats().instructions > 150000000ull &&
                    core_->GetState() == Cg50NativeCore::State::Sleeping) {
                    break;
                }
            } else {
                return false;
            }
        }
        // Dismiss "MAIN MEMORY CLEARED! Press [MENU] KEY" -> Main Menu
        TapKey(kMenu, 4, 40);
        Stabilize(6, 60);
        // Enter MicroPython app: ALPHA + S<->D ('H')
        TapKey(kAlpha, 3, 12);
        TapKey(kSD, 3, 30);
        Stabilize(6, 80);
        return core_->GetState() != Cg50NativeCore::State::Halted;
    }

    bool LoadFromPyListSnap(const std::vector<std::uint8_t>& sys,
                            const std::vector<std::uint8_t>& userWithAllPy,
                            const std::vector<std::uint8_t>& c2,
                            const std::vector<std::uint8_t>& pyListSnap)
    {
        core_ = std::make_unique<Cg50NativeCore>();
        if (!core_->LoadFirmwareParts(sys.data(), sys.size(), userWithAllPy.data(), userWithAllPy.size())) return false;
        if (!core_->LoadC2Rom(c2.data(), c2.size())) return false;
        if (!core_->ImportState(pyListSnap.data(), pyListSnap.size())) return false;
        core_->SetStrictSh7305NoFpu(true);
        return true;
    }

    void StepChunks(int count)
    {
        for (int i = 0; i < count; ++i) {
            if (core_->GetState() == Cg50NativeCore::State::Sleeping) {
                core_->AdvanceCycles(kChunkBudget);
            } else if (core_->GetState() == Cg50NativeCore::State::Running) {
                core_->RunInstructions(kChunkBudget);
            } else {
                break;
            }
        }
    }

    void TapKey(std::uint16_t code, int holdChunks, int postChunks)
    {
        core_->KeyDown(code);
        StepChunks(holdChunks);
        core_->KeyUp(code);
        StepChunks(postChunks);
    }

    bool Stabilize(int stableNeeded, int maxChunks)
    {
        std::uint64_t prev = HashDisp();
        int streak = 0;
        for (int i = 0; i < maxChunks; ++i) {
            StepChunks(1);
            const std::uint64_t cur = HashDisp();
            if (cur == prev && core_->GetState() == Cg50NativeCore::State::Sleeping) {
                if (++streak >= stableNeeded) return true;
            } else {
                streak = 0;
                prev = cur;
            }
        }
        return false;
    }

    const std::uint32_t* Disp() const
    {
        const auto& lcd = core_->Peripherals().Lcd();
        if (lcd.FramebufferAvailable() && lcd.FramebufferPixelCount() >= static_cast<std::size_t>(kPixels)) {
            const std::uint16_t* src = lcd.FramebufferData();
            for (int y = 0; y < kH; ++y) {
                const std::uint16_t* sRow = src + y * kW;
                std::uint32_t* dRow = disp_.data() + (kH - 1 - y) * kW;
                for (int x = 0; x < kW; ++x) {
                    const std::uint16_t p = sRow[x];
                    const std::uint32_t r = ((p >> 11) & 0x1Fu) * 255u / 31u;
                    const std::uint32_t g = ((p >> 5) & 0x3Fu) * 255u / 63u;
                    const std::uint32_t b = (p & 0x1Fu) * 255u / 31u;
                    dRow[kW - 1 - x] = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
            }
        }
        return disp_.data();
    }
    std::uint64_t HashDisp() const { return Fnv1a32(Disp(), kPixels); }
    Cg50NativeCore& Core() { return *core_; }

    void AnalyzePixels(int& colored, int& dark, int& uniqueColors) const
    {
        const std::uint32_t* d = Disp();
        colored = 0;
        dark = 0;
        std::set<std::uint32_t> cols;
        // Skip top 24px OS status bar when counting rendered benchmark pixels
        for (int y = 24; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                const std::uint32_t rgb = d[y * kW + x] & 0xFFFFFFu;
                cols.insert(rgb);
                const int r = static_cast<int>((rgb >> 16) & 0xFFu);
                const int g = static_cast<int>((rgb >> 8) & 0xFFu);
                const int b = static_cast<int>(rgb & 0xFFu);
                if (std::abs(r - g) > 20 || std::abs(r - b) > 20 || std::abs(g - b) > 20) ++colored;
                if (r < 100 && g < 100 && b < 100) ++dark;
            }
        }
        uniqueColors = static_cast<int>(cols.size());
    }

private:
    std::unique_ptr<Cg50NativeCore> core_;
    mutable std::vector<std::uint32_t> disp_ = std::vector<std::uint32_t>(kPixels, 0u);
};

BenchResult RunSingleBenchmark(std::size_t benchIdx,
                               const BenchSpec& spec,
                               const std::vector<std::uint8_t>& sys,
                               const std::vector<std::uint8_t>& userWithAllPy,
                               const std::vector<std::uint8_t>& c2,
                               const std::vector<std::uint8_t>& pyListSnap,
                               const fs::path& outDir)
{
    BenchResult res;
    res.id = spec.id;
    res.category = spec.category;
    res.title = spec.title;

    PyBenchSession sess;
    if (!sess.LoadFromPyListSnap(sys, userWithAllPy, c2, pyListSnap)) {
        res.notes = "LoadFromPyListSnap failed";
        return res;
    }

    // Move cursor down past the CASIO folder (1) + benchIdx rows to highlight bXX.py!
    for (std::size_t k = 0; k < benchIdx + 1u; ++k) {
        sess.TapKey(kDown, 2, 10);
    }
    sess.Stabilize(3, 25);
    const std::uint64_t preRunHash = sess.HashDisp();

    // Measure ONLY the script execution window (F1 RUN -> finished & Sleeping!)
    const auto t0 = std::chrono::steady_clock::now();
    const auto inst0 = sess.Core().GetStats().instructions;
    const auto cyc0  = sess.Core().GetStats().cycles;
    const auto auth0 = sess.Core().GetAuthenticityStats();

    sess.TapKey(kF1, 3, 20); // F1 = RUN highlighted .py script!
    bool finished = sess.Stabilize(8, spec.maxComputeChunks);

    const auto inst1 = sess.Core().GetStats().instructions;
    const auto cyc1  = sess.Core().GetStats().cycles;
    const auto auth1 = sess.Core().GetAuthenticityStats();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    res.instructions = inst1 - inst0;
    res.cycles = cyc1 - cyc0;
    res.bcdOps = auth1.bcdAluOpsExecuted - auth0.bcdAluOpsExecuted;
    res.fpuOps = auth1.fpuOpcodesExecuted - auth0.fpuOpcodesExecuted;
    res.dspOps = auth1.dspOpcodesExecuted - auth0.dspOpcodesExecuted;
    res.elapsedMs = ms;
    res.mips = (ms > 0.0) ? (static_cast<double>(res.instructions) / (ms * 1000.0)) : 0.0;
    res.fbHash = sess.HashDisp();
    sess.AnalyzePixels(res.coloredPixels, res.darkPixels, res.uniqueColors);

    const fs::path bmp = outDir / (spec.id + ".bmp");
    WriteBmp24(bmp, sess.Disp());
    res.bmpPath = bmp.string();

    const bool notHalted = (sess.Core().GetState() != Cg50NativeCore::State::Halted);
    const bool screenChanged = (res.fbHash != preRunHash);
    res.passed = finished && notHalted && screenChanged && res.fpuOps == 0u &&
                 (res.coloredPixels >= spec.minColoredPixels || res.darkPixels >= spec.minColoredPixels) &&
                 res.uniqueColors >= spec.minUniqueColors;
    if (!screenChanged) res.notes = "Screen did not change from Python file list (script not executed)";
    else if (!finished) res.notes = "Did not reach Sleeping/stable within maxComputeChunks";
    else if (!notHalted) res.notes = "Core halted unexpectedly";
    else if (res.fpuOps != 0u) res.notes = "Forbidden FPU opcode executed on SH7305";
    else if (!res.passed) res.notes = "Rendered pixels below threshold (possible MicroPython syntax/runtime error)";
    else res.notes = "CONFIRMED native Casio OS 3.80 MicroPython casioplot render";
    return res;
}

} // namespace

int main(int argc, char** argv)
{
    fs::path romDir = "D:/Proyectos/CalculadorasUnificadas/assets/roms";
    fs::path outDir = "D:/Proyectos/CalculadorasUnificadas/tests/cg50_validation/reports/graphics_v46";
    unsigned threads = std::min<unsigned>(6u, std::max<unsigned>(1u, std::thread::hardware_concurrency()));

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--roms") == 0 && i + 1 < argc) romDir = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) outDir = argv[++i];
        else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) threads = static_cast<unsigned>(std::max(1, std::atoi(argv[++i])));
    }

    std::vector<std::string> badEnvs;
    if (Cg50NativeCore::AuditForbiddenEnvOverrides(badEnvs) != 0u) {
        std::fprintf(stderr, "[GFX-BENCH] FAIL: forbidden HLE env overrides detected!\n");
        return 2;
    }

    std::vector<std::uint8_t> sys, user, c2;
    if (!ReadFileBytes(romDir / "SystemROM.raw", sys) ||
        !ReadFileBytes(romDir / "UserROM.raw", user) ||
        !ReadFileBytes(romDir / "C2ROM.raw", c2)) {
        std::fprintf(stderr, "[GFX-BENCH] FAIL: unable to read ROMs from %s\n", romDir.string().c_str());
        return 2;
    }

    fs::create_directories(outDir);
    const auto specs = BuildBenchmarks();

    // 1. Inject ALL 11 benchmark scripts (/b01.py .. /b11.py) into UserROM via Fugue FTL + FAT16 BEFORE cold boot!
    {
        cg50::Cg50SmemFs smem;
        if (!smem.Mount(user.data(), user.size(), c2.data(), c2.size())) {
            std::fprintf(stderr, "[GFX-BENCH] FAIL: SMEM mount failed: %s\n", smem.LastError().c_str());
            return 3;
        }
        std::vector<cg50::SmemEntry> entries;
        if (smem.ListDirectory("/", entries)) {
            for (const auto& e : entries) {
                if (!e.isDir && e.name.size() >= 3 &&
                    (e.name.substr(e.name.size() - 3) == ".py" || e.name.substr(e.name.size() - 3) == ".PY")) {
                    (void)smem.DeletePath(e.fullPath, false);
                }
            }
        }
        for (std::size_t i = 0; i < specs.size(); ++i) {
            char fname[32];
            std::snprintf(fname, sizeof(fname), "/b%02zu.py", i + 1u);
            if (!smem.WriteFile(fname, reinterpret_cast<const std::uint8_t*>(specs[i].script.data()),
                                specs[i].script.size(), true)) {
                std::fprintf(stderr, "[GFX-BENCH] FAIL: WriteFile(%s) failed: %s\n", fname, smem.LastError().c_str());
                return 3;
            }
        }
    }

    // 2. Single cold boot into Casio OS 3.80 Python app so Fugue FTL discovers all 11 scripts cleanly!
    std::printf("[GFX-BENCH] Cold-booting Casio OS 3.80 with 11 injected MicroPython benchmarks...\n");
    std::fflush(stdout);
    const auto tStart = std::chrono::steady_clock::now();
    PyBenchSession master;
    if (!master.LoadColdIntoPythonList(sys, user, c2)) {
        std::fprintf(stderr, "[GFX-BENCH] FAIL: cold boot into Python app failed\n");
        return 4;
    }
    WriteBmp24(outDir / "00_python_file_list.bmp", master.Disp());
    std::vector<std::uint8_t> pyListSnap;
    if (!master.Core().ExportState(pyListSnap)) {
        std::fprintf(stderr, "[GFX-BENCH] FAIL: ExportState(pyListSnap) failed\n");
        return 4;
    }
    std::printf("[GFX-BENCH] Python file list ready (snap=%zu bytes). Launching %zu benchmarks across %u threads...\n",
                pyListSnap.size(), specs.size(), threads);
    std::fflush(stdout);

    std::vector<BenchResult> results(specs.size());
    std::atomic<std::size_t> nextIdx{0};
    auto worker = [&]() {
        for (;;) {
            const std::size_t idx = nextIdx.fetch_add(1u);
            if (idx >= specs.size()) break;
            results[idx] = RunSingleBenchmark(idx, specs[idx], sys, user, c2, pyListSnap, outDir);
            const auto& r = results[idx];
            std::printf("[GFX-BENCH] %-18s | %s | inst=%11llu | %7.1f ms (%5.1f MIPS) | colored=%5d unique=%4d | hash=0x%016llX | fpu=%llu\n",
                        r.id.c_str(), r.passed ? "PASS" : "FAIL",
                        static_cast<unsigned long long>(r.instructions),
                        r.elapsedMs, r.mips, r.coloredPixels, r.uniqueColors,
                        static_cast<unsigned long long>(r.fbHash),
                        static_cast<unsigned long long>(r.fpuOps));
            std::fflush(stdout);
        }
    };

    const unsigned active = std::min<unsigned>(threads, static_cast<unsigned>(specs.size()));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < active; ++t) pool.emplace_back(worker);
    for (auto& th : pool) th.join();

    const double totalSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
    int passedCnt = 0;
    std::uint64_t totalInst = 0, totalFpu = 0;
    std::set<std::uint64_t> uniqueHashes;
    for (const auto& r : results) {
        if (r.passed) ++passedCnt;
        totalInst += r.instructions;
        totalFpu += r.fpuOps;
        uniqueHashes.insert(r.fbHash);
    }
    const bool allDistinct = (uniqueHashes.size() == results.size());

    std::ofstream jf(outDir / "graphics_compute_benchmarks_v46.json", std::ios::binary);
    jf << "{\n"
       << "  \"schema\": \"cg50_graphics_compute_benchmarks_v46\",\n"
       << "  \"strict_no_hle\": true,\n"
       << "  \"strict_sh7305_no_fpu\": true,\n"
       << "  \"threads\": " << active << ",\n"
       << "  \"total_benchmarks\": " << results.size() << ",\n"
       << "  \"passed\": " << passedCnt << ",\n"
       << "  \"failed\": " << (static_cast<int>(results.size()) - passedCnt) << ",\n"
       << "  \"total_instructions\": " << totalInst << ",\n"
       << "  \"total_fpu_opcodes\": " << totalFpu << ",\n"
       << "  \"wall_time_sec\": " << std::fixed << std::setprecision(2) << totalSec << ",\n"
       << "  \"benchmarks\": [\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        jf << "    {\n"
           << "      \"id\": \"" << JsonEscape(r.id) << "\",\n"
           << "      \"category\": \"" << JsonEscape(r.category) << "\",\n"
           << "      \"title\": \"" << JsonEscape(r.title) << "\",\n"
           << "      \"passed\": " << (r.passed ? "true" : "false") << ",\n"
           << "      \"instructions\": " << r.instructions << ",\n"
           << "      \"cycles\": " << r.cycles << ",\n"
           << "      \"bcd_alu_ops\": " << r.bcdOps << ",\n"
           << "      \"fpu_opcodes\": " << r.fpuOps << ",\n"
           << "      \"dsp_opcodes\": " << r.dspOps << ",\n"
           << "      \"elapsed_ms\": " << std::fixed << std::setprecision(2) << r.elapsedMs << ",\n"
           << "      \"mips\": " << std::fixed << std::setprecision(2) << r.mips << ",\n"
           << "      \"colored_pixels\": " << r.coloredPixels << ",\n"
           << "      \"dark_pixels\": " << r.darkPixels << ",\n"
           << "      \"unique_colors\": " << r.uniqueColors << ",\n"
           << "      \"fb_hash\": \"0x" << std::hex << std::uppercase << r.fbHash << std::dec << "\",\n"
           << "      \"bmp_path\": \"" << JsonEscape(r.bmpPath) << "\",\n"
           << "      \"notes\": \"" << JsonEscape(r.notes) << "\"\n"
           << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    jf << "  ]\n}\n";
    jf.close();

    std::printf("[GFX-BENCH] SUMMARY: %d / %zu PASSED | distinct_hashes=%zu/%zu | total_inst=%llu | total_fpu=%llu | wall=%.2fs\n",
                passedCnt, results.size(), uniqueHashes.size(), results.size(),
                static_cast<unsigned long long>(totalInst),
                static_cast<unsigned long long>(totalFpu), totalSec);
    return (passedCnt == static_cast<int>(results.size()) && allDistinct && totalFpu == 0u) ? 0 : 1;
}
