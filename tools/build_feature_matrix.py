#!/usr/bin/env python3
"""
Build a small feature-flag matrix for the WebUI and firmware, report bundle/flash sizes,
and restore platformio.ini afterwards.

Usage:
  python tools/build_feature_matrix.py --env usb --quick
  python tools/build_feature_matrix.py --env usb

Flags covered (per [env:<name>] build_flags):
    -DCM_EMBED_WEBUI
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Tuple

ROOT = Path(__file__).resolve().parents[1]
PIO_INI = ROOT / 'platformio.ini'

FLAGS = [
    'CM_EMBED_WEBUI',
]


def read_lines() -> List[str]:
    return PIO_INI.read_text(encoding='utf-8', errors='ignore').splitlines(keepends=True)


def write_lines(lines: List[str]):
    PIO_INI.write_text(''.join(lines), encoding='utf-8')


def find_env_block(lines: List[str], env_name: str) -> Tuple[int, int]:
    start = end = -1
    hdr = f'[env:{env_name}]'
    for i, ln in enumerate(lines):
        s = ln.strip()
        if s.startswith('[') and s.endswith(']'):
            if start >= 0:
                end = i
                break
            if s == hdr:
                start = i
    if start >= 0 and end < 0:
        end = len(lines)
    return start, end


def find_build_flags_block(lines: List[str], env_start: int, env_end: int) -> Tuple[int, int]:
    if env_start < 0:
        return -1, -1
    start = end = -1
    i = env_start
    while i < env_end:
        ln = lines[i]
        if ln.strip().startswith('build_flags') and '=' in ln:
            start = i
            i += 1
            # Collect continuation lines: indented or empty lines until next key (=) at col 0
            while i < env_end:
                raw = lines[i]
                s = raw.strip()
                is_key_line = (not raw.startswith((' ', '\t', ';', '#'))) and ('=' in raw)
                if s and is_key_line:
                    break
                i += 1
            end = i
            break
        i += 1
    return start, end


def tokens_from_build_flags(lines: List[str], blk_start: int, blk_end: int) -> List[str]:
    """Extract tokens from build_flags block (excluding the 'build_flags =' header line)."""
    tokens: List[str] = []
    for i in range(blk_start + 1, blk_end):
        s = lines[i].strip()
        if s and not s.startswith((';', '#')):
            tokens.extend(s.split())
    return tokens


def apply_overrides(tokens: List[str], overrides: Dict[str, int]) -> List[str]:
    """Return a new tokens list where -D<FLAG>=<0|1> are set/updated per overrides."""
    out: List[str] = []
    seen = set()
    for t in tokens:
        replaced = False
        if t.startswith('-D'):
            kv = t[2:]
            name = kv.split('=', 1)[0]
            if name in overrides:
                out.append(f"-D{name}={overrides[name]}")
                seen.add(name)
                replaced = True
        if not replaced:
            out.append(t)
    for name, val in overrides.items():
        if name not in seen:
            out.append(f"-D{name}={val}")
    return out


def build_flags_for_env(env_name: str, config: Dict[str, int]) -> str:
    """Read base build_flags for env_name and apply overrides; return a single-line string suitable for -O."""
    lines = read_lines()
    env_start, env_end = find_env_block(lines, env_name)
    if env_start < 0:
        raise RuntimeError(f"Environment [env:{env_name}] not found in platformio.ini")
    blk_start, blk_end = find_build_flags_block(lines, env_start, env_end)
    if blk_start < 0:
        raise RuntimeError(f"build_flags not found in [env:{env_name}] block")
    tokens = tokens_from_build_flags(lines, blk_start, blk_end)
    tokens = apply_overrides(tokens, config)
    return ' '.join(tokens)


def run_build(env_name: str, build_flags: str, timeout: int = 900) -> str:
    """Run pio build with overrides; prefer --project-option, fallback to env var for extra_script-only."""
    # Attempt 1: Use --project-option (non-destructive override)
    proc = subprocess.run(
        ['pio', 'run', '-e', env_name, '--project-option', f'build_flags = {build_flags}'],
        cwd=str(ROOT), capture_output=True, text=True, timeout=timeout
    )
    if proc.returncode == 0:
        return proc.stdout + proc.stderr

    # Fallback: retry by setting env var so extra_script picks up flags for frontend pruning
    env = os.environ.copy()
    env['PLATFORMIO_BUILD_FLAGS'] = build_flags
    proc2 = subprocess.run(
        ['pio', 'run', '-e', env_name],
        cwd=str(ROOT), capture_output=True, text=True, timeout=timeout, env=env
    )
    if proc2.returncode != 0:
        sys.stdout.write(proc.stdout + proc2.stdout)
        sys.stderr.write(proc.stderr + proc2.stderr)
        raise RuntimeError(f"Build failed for env {env_name}")
    # Note: In fallback, C++ flags may not change; frontend pruning still applies via extra_script
    return proc2.stdout + proc2.stderr


def parse_metrics(output: str) -> Dict[str, str]:
    metrics: Dict[str, str] = {}
    # Vite JS bundle gzip size
    m = re.search(r"dist/assets/index-\S+\.js\s+(\d+\.\d+)\s*kB\s*\|\s*gzip:\s*(\d+\.\d+)\s*kB", output)
    if m:
        metrics['js_kb'] = m.group(1)
        metrics['js_gzip_kb'] = m.group(2)
    m = re.search(r"dist/assets/style-\S+\.css\s+(\d+\.\d+)\s*kB\s*\|\s*gzip:\s*(\d+\.\d+)\s*kB", output)
    if m:
        metrics['css_kb'] = m.group(1)
        metrics['css_gzip_kb'] = m.group(2)
    # Flash usage and percent
    m = re.search(r"Flash:\s*\[[^\]]+\]\s*(\d+\.\d+)%\s*\(used\s*(\d+)\s*bytes\s*from\s*(\d+)\s*bytes\)", output)
    if m:
        metrics['flash_pct'] = m.group(1)
        metrics['flash_used'] = m.group(2)
        metrics['flash_total'] = m.group(3)
    return metrics


def format_config_label(cfg: Dict[str, int]) -> str:
    parts = []
    for f in FLAGS:
        parts.append(f"{f.lower()}={'on' if cfg.get(f, 0) else 'off'}")
    return ', '.join(parts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--env', default='usb', help='PlatformIO environment name (default: usb)')
    ap.add_argument('--quick', action='store_true', help='Run a reduced matrix (baseline + all-off)')
    args = ap.parse_args()

    if not PIO_INI.exists():
        print('platformio.ini not found')
        return 1
    results = []
    baseline = {f: 1 for f in FLAGS}
    all_off = {f: 0 for f in FLAGS}
    variants: List[Dict[str, int]]
    if args.quick:
        variants = [baseline, all_off]
    else:
        variants = [baseline]
        # Single-flag off variants
        for f in FLAGS:
            cfg = baseline.copy()
            cfg[f] = 0
            variants.append(cfg)
        variants.append(all_off)

    for idx, cfg in enumerate(variants, 1):
        bf = build_flags_for_env(args.env, cfg)
        print(f"\n=== Build {idx}/{len(variants)}: {format_config_label(cfg)} ===")
        out = run_build(args.env, bf)
        metrics = parse_metrics(out)
        results.append((cfg, metrics))

    # Report summary
    print("\nSummary:")
    for cfg, m in results:
        label = format_config_label(cfg)
        js = f"JS {m.get('js_kb','?')} kB (gzip {m.get('js_gzip_kb','?')} kB)"
        css = f"CSS {m.get('css_kb','?')} kB (gzip {m.get('css_gzip_kb','?')} kB)"
        flash = f"Flash {m.get('flash_pct','?')}% ({m.get('flash_used','?')}/{m.get('flash_total','?')} bytes)"
        print(f"- {label}: {js}, {css}, {flash}")

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
