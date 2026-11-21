#!/usr/bin/env python3
"""Quick reverb analysis of a stereo WAV (impulse response renders).

usage: analyze.py file.wav [--png out.png] [--ir]
Prints RT60 (broadband + octave bands), NED onset, resonance peaks in the
tail, envelope flutter, L/R correlation, mono energy ratio, peak and DC.
"""
import sys, argparse
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as sig


def load(path):
    fs, x = wav.read(path)
    x = x.astype(np.float64)
    if x.ndim == 1:
        x = np.stack([x, x], 1)
    return fs, x


def edc_db(h):
    e = np.cumsum((h ** 2)[::-1])[::-1]
    e = e / max(e[0], 1e-30)
    return 10 * np.log10(np.maximum(e, 1e-30))


def t30(h, fs):
    d = edc_db(h)
    try:
        i5 = np.where(d <= -5)[0][0]
        i35 = np.where(d <= -35)[0][0]
    except IndexError:
        return float('nan')
    t = np.arange(len(d)) / fs
    p = np.polyfit(t[i5:i35], d[i5:i35], 1)
    return -60.0 / p[0]


def ned(h, fs, win=0.02):
    n = int(win * fs)
    out = []
    for i in range(0, len(h) - n, n // 2):
        w = h[i:i + n]
        sd = np.std(w)
        if sd < 1e-12:
            out.append(0.0)
            continue
        out.append(np.mean(np.abs(w) > sd) / 0.3173)
    return np.array(out), n // 2 / fs


def tail_resonances(h, fs, start, length=1.0):
    a = int(start * fs)
    b = min(len(h), a + int(length * fs))
    seg = h[a:b]
    if len(seg) < 4096:
        return []
    f, p = sig.welch(seg, fs, nperseg=8192)
    pdb = 10 * np.log10(np.maximum(p, 1e-30))
    # smoothed envelope over 1/3 octave
    sm = np.copy(pdb)
    for i in range(1, len(f)):
        lo, hi = f[i] / 2 ** (1 / 6), f[i] * 2 ** (1 / 6)
        m = (f >= lo) & (f <= hi)
        sm[i] = 10 * np.log10(np.mean(10 ** (pdb[m] / 10)))
    excess = pdb - sm
    sel = (f > 80) & (f < 12000)
    idx = np.argsort(excess[sel])[::-1][:5]
    return [(float(f[sel][i]), float(excess[sel][i])) for i in idx]


def flutter(h, fs, start, length=1.5):
    a = int(start * fs)
    b = min(len(h), a + int(length * fs))
    seg = h[a:b]
    if len(seg) < fs * 0.5:
        return float('nan')
    env = np.sqrt(sig.lfilter([1], [1, -0.99], seg ** 2) * 0.01 + 1e-30)
    env = 20 * np.log10(env)
    # remove the linear decay trend
    t = np.arange(len(env))
    env = env - np.polyval(np.polyfit(t, env, 1), t)
    env = env[::48]
    env = env - env.mean()
    ac = np.correlate(env, env, 'full')[len(env) - 1:]
    ac = ac / max(ac[0], 1e-30)
    lag0, lag1 = int(0.02 * fs / 48), int(0.25 * fs / 48)
    return float(np.max(ac[lag0:lag1]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('wav')
    ap.add_argument('--png')
    a = ap.parse_args()
    fs, x = load(a.wav)
    L, R = x[:, 0], x[:, 1]
    m = 0.5 * (L + R)
    print(f'file {a.wav}  fs={fs}  len={len(L)/fs:.1f}s  peak={np.max(np.abs(x)):.3f}  rms={np.sqrt(np.mean(x**2)):.4f}  dc={np.mean(x):.2e}')
    if not np.all(np.isfinite(x)):
        print('  NON-FINITE samples!')
    rt = t30(L + R, fs)
    print(f'  T30 broadband: {rt:.2f} s')
    bands = [125, 250, 500, 1000, 2000, 4000, 8000]
    out = []
    for fc in bands:
        b, aa = sig.butter(3, [fc / 2 ** 0.5, min(fc * 2 ** 0.5, fs * 0.45)], 'bandpass', fs=fs)
        out.append(t30(sig.lfilter(b, aa, L + R), fs))
    print('  T30 bands: ' + '  '.join(f'{f}:{v:.2f}' for f, v in zip(bands, out)))
    nd, hop = ned(L, fs)
    first = np.argmax(np.abs(L) > 1e-4 * np.max(np.abs(L)))
    t0 = first / fs
    above = np.where(nd > 0.9)[0]
    onset = above[0] * hop if len(above) else float('nan')
    print(f'  first arrival {t0*1000:.0f} ms, NED>0.9 at {onset*1000:.0f} ms (rel {1000*(onset-t0):.0f} ms), NED mean 0.3-1s: {np.mean(nd[int(0.3/hop):int(1/hop)]):.2f}')
    st = min(max(0.5, rt * 0.25), len(L) / fs - 1.2)
    print('  tail resonance peaks (Hz, dB over 1/3 oct): ' + ', '.join(f'{f:.0f}:{e:.1f}' for f, e in tail_resonances(m, fs, st)))
    print(f'  flutter (env autocorr 20-250 ms): {flutter(m, fs, st):.2f}')
    a0, b0 = int(st * fs), int(min(len(L), (st + 2) * fs))
    l, r = L[a0:b0], R[a0:b0]
    rho = np.sum(l * r) / np.sqrt(np.sum(l * l) * np.sum(r * r) + 1e-30)
    mono = np.sum((l + r) ** 2) / (np.sum(l * l) + np.sum(r * r) + 1e-30)
    print(f'  tail L/R correlation {rho:.3f}, mono/(L^2+R^2) energy {10*np.log10(mono+1e-30):.2f} dB')
    if a.png:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        f, t, S = sig.spectrogram(m, fs, nperseg=2048, noverlap=1536)
        fig, ax = plt.subplots(2, 1, figsize=(10, 6), gridspec_kw={'height_ratios': [3, 1]})
        ax[0].pcolormesh(t, f, 10 * np.log10(S + 1e-14), shading='auto', vmin=-140, vmax=-40, cmap='magma')
        ax[0].set_yscale('symlog', linthresh=500)
        ax[0].set_ylim(30, fs / 2)
        ax[0].set_title(a.wav)
        env = 10 * np.log10(sig.lfilter([1], [1, -0.999], m ** 2) * 0.001 + 1e-14)
        ax[1].plot(np.arange(len(m)) / fs, env)
        ax[1].set_ylim(-140, 0)
        plt.tight_layout()
        plt.savefig(a.png, dpi=70)


if __name__ == '__main__':
    main()
