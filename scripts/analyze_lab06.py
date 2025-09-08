#!/usr/bin/env python3
import subprocess, sys, csv, time, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN  = ROOT / "bin"
OUT  = ROOT / "data"
FIGS = ROOT / "docs" / "figs"
OUT.mkdir(parents=True, exist_ok=True)
FIGS.mkdir(parents=True, exist_ok=True)

# ---------- util ----------
def run(cmd, timeout=600):
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout)
    return p.returncode, p.stdout

def median(xs):
    s = sorted(xs)
    n = len(s)
    return 0 if n == 0 else (s[n//2] if n%2 else 0.5*(s[n//2-1]+s[n//2]))

# ---------- P1: counter ----------

_rx_p1 = re.compile(r"Resumen \(ms\):\s*naive=([\d\.]+)\s+mutex=([\d\.]+)\s+sharded=([\d\.]+)\s+atomic=([\d\.]+)")

def parse_p1(stdout):
    m = None
    for line in stdout.splitlines()[::-1]:
        m = _rx_p1.search(line)
        if m: break
    if not m:
        raise RuntimeError("No encontré la línea de resumen en la salida de p1_counter")
    naive, mutex, sharded, atomic = map(float, m.groups())
    return {"naive_ms":naive, "mutex_ms":mutex, "sharded_ms":sharded, "atomic_ms":atomic}

def bench_p1(iters=1_000_000, threads_list=(1,2,4,8), reps=3):
    rows = []
    for T in threads_list:
        vals = {"naive_ms":[], "mutex_ms":[], "sharded_ms":[], "atomic_ms":[]}
        for _ in range(reps):
            code, out = run([str(BIN/"p1_counter"), str(T), str(iters)])
            if code != 0:
                print(out)
                raise RuntimeError("p1_counter falló")
            r = parse_p1(out)
            for k,v in r.items(): vals[k].append(v)
        row = {"threads":T}
        for k in vals: row[k] = round(median(vals[k]), 4)
        rows.append(row)

    csv_path = OUT/"p1_times.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["threads","naive_ms","mutex_ms","sharded_ms","atomic_ms"])
        w.writeheader(); w.writerows(rows)
    print(f"[OK] Guardado {csv_path}")
    return rows

# ---------- P5: pipeline ----------
_rx_ms = re.compile(r"tiempo\s*=\s*([\d\.]+)\s*ms", re.IGNORECASE)

def parse_or_time(cmd):
    t0 = time.perf_counter()
    code, out = run(cmd)
    t1 = time.perf_counter()
    if code != 0:
        print(out)
        raise RuntimeError("p5_pipeline falló")
    m = None
    for line in out.splitlines()[::-1]:
        m = _rx_ms.search(line)
        if m: break
    if m:
        return float(m.group(1)), out
    # fallback a wall-clock (ms)
    return (t1 - t0)*1000.0, out

def bench_p5(messages_list=(20000, 50000), buffer_list=(8,16,32,64,128), pause_us_list=(0, 500, 2000), reps=3):
    rows = []
    for msgs in messages_list:
        for buf in buffer_list:
            for pause in pause_us_list:
                times = []
                for _ in range(reps):
                    ms, out = parse_or_time([str(BIN/"p5_pipeline"), str(msgs), str(buf), str(pause)])
                    times.append(ms)
                rows.append({
                    "messages": msgs,
                    "buffer": buf,
                    "pause_us": pause,
                    "time_ms": round(median(times), 4)
                })
    csv_path = OUT/"p5_times.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["messages","buffer","pause_us","time_ms"])
        w.writeheader(); w.writerows(rows)
    print(f"[OK] Guardado {csv_path}")
    return rows

# ---------- plots ----------
def plot_p1(rows):
    import matplotlib.pyplot as plt
    xs = [r["threads"] for r in rows]
    for key,label in [("naive_ms","Naive"), ("mutex_ms","Mutex"), ("sharded_ms","Sharded"), ("atomic_ms","Atomic")]:
        ys = [r[key] for r in rows]
        plt.figure()
        plt.plot(xs, ys, marker="o")
        plt.xlabel("Hilos (T)")
        plt.ylabel("Tiempo (ms)")
        plt.title(f"Práctica 1 — {label} vs hilos")
        plt.grid(True)
        fig_path = FIGS/f"p1_{key}.png"
        plt.savefig(fig_path, dpi=160, bbox_inches="tight")
        plt.close()
        print(f"[OK] Figura {fig_path}")

def plot_p5(rows, fixed_messages=None, fixed_pause=None):
    import matplotlib.pyplot as plt
    # gráfico 1: tiempo vs buffer 
    if fixed_messages is None: fixed_messages = rows[0]["messages"]
    if fixed_pause is None: fixed_pause = rows[0]["pause_us"]
    subset = [r for r in rows if r["messages"]==fixed_messages and r["pause_us"]==fixed_pause]
    subset = sorted(subset, key=lambda r:r["buffer"])
    xs = [r["buffer"] for r in subset]
    ys = [r["time_ms"] for r in subset]
    plt.figure()
    plt.plot(xs, ys, marker="o")
    plt.xlabel("Tamaño de buffer")
    plt.ylabel("Tiempo total (ms)")
    plt.title(f"Práctica 5 — Tiempo vs buffer (messages={fixed_messages}, pause_us={fixed_pause})")
    plt.grid(True)
    fig_path = FIGS/f"p5_time_vs_buffer_m{fixed_messages}_p{fixed_pause}.png"
    plt.savefig(fig_path, dpi=160, bbox_inches="tight")
    plt.close()
    print(f"[OK] Figura {fig_path}")

    # gráfico 2: tiempo vs pause_us 
    common_buf = sorted({r["buffer"] for r in rows})[0]
    subset2 = [r for r in rows if r["messages"]==fixed_messages and r["buffer"]==common_buf]
    subset2 = sorted(subset2, key=lambda r:r["pause_us"])
    xs2 = [r["pause_us"] for r in subset2]
    ys2 = [r["time_ms"] for r in subset2]
    plt.figure()
    plt.plot(xs2, ys2, marker="o")
    plt.xlabel("pause_us")
    plt.ylabel("Tiempo total (ms)")
    plt.title(f"Práctica 5 — Tiempo vs pause_us (messages={fixed_messages}, buffer={common_buf})")
    plt.grid(True)
    fig_path = FIGS/f"p5_time_vs_pause_m{fixed_messages}_b{common_buf}.png"
    plt.savefig(fig_path, dpi=160, bbox_inches="tight")
    plt.close()
    print(f"[OK] Figura {fig_path}")

def main():

    threads_list = (1,2,4,8)
    iters = 1_000_000
    messages_list = (20000, 50000)
    buffer_list = (8,16,32,64,128)
    pause_us_list = (0, 500, 2000)
    reps = 3

    # P1
    print("=== Benchmark P1 (counter) ===")
    p1 = bench_p1(iters=iters, threads_list=threads_list, reps=reps)
    try:
        plot_p1(p1)
    except Exception as e:
        print("[WARN] No se pudo graficar P1:", e)

    # P5
    print("=== Benchmark P5 (pipeline) ===")
    p5 = bench_p5(messages_list=messages_list, buffer_list=buffer_list, pause_us_list=pause_us_list, reps=reps)
    try:
        # fija algún caso típico para el primer gráfico
        plot_p5(p5, fixed_messages=messages_list[-1], fixed_pause=pause_us_list[0])
    except Exception as e:
        print("[WARN] No se pudo graficar P5:", e)

if __name__ == "__main__":
    main()
