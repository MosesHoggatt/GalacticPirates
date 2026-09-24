#!/usr/bin/env python3
"""Extract specific WAVs from Sonniss GDC zip parts, then delete the zips."""

from __future__ import annotations

import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

USER_AGENT = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36"
)

ZIP_URLS = [
    f"https://downloads.sonniss.com/Sonniss.com-GDC2024-GameAudioBundle{i}of9.zip"
    for i in range(1, 10)
]

# Exact 2024 tracklist filenames -> in-game polish clip names.
RENAME = {
    "Bluezone_BC0304_retrofuturistic_computer_alarm_005.wav": "SFX_Alarm.wav",
    "EXPLDsgn_Nuclear Explosion 07_DDUMAIS_NONE.wav": "SFX_Explosion.wav",
    "Bluezone_BC0294_modern_cinematic_impact_boom_003.wav": "SFX_ExplosionBass.wav",
    "Bluezone_BC0294_modern_cinematic_impact_022.wav": "SFX_Impact.wav",
    "SCIWeap_Shot Pulse YR 05_RSCPC_SFEW.wav": "SFX_PulseFire.wav",
    "Bluezone_BC0303_futuristic_user_interface_transition_006.wav": "SFX_Helm.wav",
    "Bluezone_BC0303_futuristic_user_interface_data_glitch_003.wav": "SFX_Denied.wav",
    "Bluezone_BC0303_futuristic_user_interface_high_tech_beep_038.wav": "SFX_Terminal.wav",
    "Bluezone_BC0303_futuristic_user_interface_alert_003.wav": "SFX_RechargeReady.wav",
    "Bluezone_BC0295_sci_fi_weapon_gun_shot_008.wav": "SFX_Minigun.wav",
    "EXPLReal_Medium Realistic Explosion 15_DDUMAIS_NONE.wav": "SFX_ExplosionHit.wav",
    "DESTRCrsh_Designed Car Explosion With Metal Breaking And Glass Shattering  06_DDUMAIS_NONE.wav": "SFX_ExplosionWreck.wav",
    "Bluezone_BC0296_steampunk_weapon_flare_shot_explosion_003.wav": "SFX_ExplosionFizzle.wav",
}


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(req, timeout=120) as resp, dest.open("wb") as out:
        total = int(resp.headers.get("Content-Length") or 0)
        copied = 0
        last_pct = -1
        while True:
            chunk = resp.read(1024 * 1024)
            if not chunk:
                break
            out.write(chunk)
            copied += len(chunk)
            if total:
                pct = int(copied * 100 / total)
                if pct != last_pct and pct % 5 == 0:
                    print(f"  {pct}% ({copied / (1024 ** 3):.2f} / {total / (1024 ** 3):.2f} GB)", flush=True)
                    last_pct = pct
    print(f"  saved {dest} ({dest.stat().st_size / (1024 ** 3):.2f} GB)", flush=True)


def extract_matches(zip_path: Path, wanted: set[str], staging: Path) -> list[str]:
    found: list[str] = []
    with zipfile.ZipFile(zip_path) as archive:
        for info in archive.infolist():
            base = Path(info.filename.replace("\\", "/")).name
            if base in wanted and not info.is_dir():
                target = staging / base
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as src, target.open("wb") as dst:
                    shutil.copyfileobj(src, dst)
                found.append(base)
                print(f"  extracted {base} ({target.stat().st_size} bytes)", flush=True)
    return found


def main() -> int:
    project = Path(__file__).resolve().parents[1]
    out_dir = project / "Content" / "Polish" / "Audio"
    cache_dir = project / "Saved" / "SonnissCache"
    out_dir.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(parents=True, exist_ok=True)

    remaining = {name: mapped for name, mapped in RENAME.items() if not (out_dir / mapped).exists()}
    if not remaining:
        print("All target SFX already exist in", out_dir)
        return 0
    print("Need:", ", ".join(sorted(remaining.values())), flush=True)
    staging = Path(tempfile.mkdtemp(prefix="sonniss_wav_", dir=str(cache_dir)))

    try:
        for url in ZIP_URLS:
            if not remaining:
                break
            zip_path = cache_dir / Path(url).name
            try:
                download(url, zip_path)
                extracted = extract_matches(zip_path, set(remaining), staging)
                for base in extracted:
                    mapped = remaining.pop(base, None)
                    if mapped:
                        shutil.copy2(staging / base, out_dir / mapped)
                        print(f"  -> {mapped}", flush=True)
            finally:
                if zip_path.exists():
                    zip_path.unlink()
                    print(f"  deleted {zip_path.name}", flush=True)
    finally:
        shutil.rmtree(staging, ignore_errors=True)

    missing = sorted(remaining.values())
    if missing:
        print("MISSING:", ", ".join(missing), file=sys.stderr)
        return 1
    print("Done. Extracted all SFX to", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
