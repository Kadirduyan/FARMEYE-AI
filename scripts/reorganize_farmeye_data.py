from pathlib import Path
import shutil
import os

ROOT = Path(r"C:\FarmEyeData")

# Hedef — sadece 3 ana klasör
FOTO = ROOT / "inek_fotograflari"
VID = ROOT / "videolar"
KAYIT = ROOT / "kayitlar"
YANDAN = FOTO / "yandan"
ONDEN = FOTO / "onden"
GUNLUK = KAYIT / "gunluk_videolar"
VERILER = KAYIT / "veriler"

for p in [FOTO, VID, KAYIT, YANDAN, ONDEN, GUNLUK, VERILER]:
    p.mkdir(parents=True, exist_ok=True)


def safe_move_file(src: Path, dst: Path):
    if not src.exists() or not src.is_file():
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        print(f"SKIP exists {dst.name}")
        return
    try:
        shutil.move(str(src), str(dst))
        print(f"OK file {src.name} -> {dst}")
    except Exception as e:
        try:
            shutil.copy2(str(src), str(dst))
            print(f"OK copy {src.name} -> {dst} ({e})")
        except Exception as e2:
            print(f"FAIL {src}: {e2}")


def safe_merge_dir(src: Path, dst: Path):
    if not src.exists() or not src.is_dir():
        return
    if src.resolve() == dst.resolve():
        return
    dst.mkdir(parents=True, exist_ok=True)
    for child in list(src.iterdir()):
        target = dst / child.name
        try:
            if child.is_file():
                if not target.exists():
                    shutil.copy2(str(child), str(target))
                    print(f"OK copy file {child.name}")
            elif child.is_dir():
                safe_merge_dir(child, target)
        except Exception as e:
            print(f"SKIP {child}: {e}")
    # Eski klasörü silmeye çalışma (izin sorunlu olabilir) — sadece işaretle
    marker = src / "_TASINDI_YENI_YERE_BAKIN.txt"
    try:
        marker.write_text(
            f"Icerik {dst} altina kopyalandi. Bu eski klasoru manuel silebilirsin.\n",
            encoding="utf-8",
        )
    except Exception:
        pass


print("=== 1) Kayit dosyalari ===")
for name in ["farmeye.db", "farmeye_demo_ornek.db", "people.db", "person_identities.json"]:
    safe_move_file(ROOT / name, VERILER / name)

print("=== 2) Gunluk videolar ===")
if (ROOT / "videos").exists():
    safe_merge_dir(ROOT / "videos", GUNLUK)

print("=== 3) Dataset videolar ===")
safe_merge_dir(ROOT / "02_cbvd5_videos", VID / "cbvd5")
safe_merge_dir(ROOT / "03_cbvd5_labels", VID / "cbvd5_etiket")
safe_merge_dir(ROOT / "04_models_cache", VID / "model_cache")

print("=== 4) Inek fotograflari (yandan) ===")
# Icerik izni sorunlu olabilir — kopyala / junction dene
old_foto = ROOT / "01_cattle_side_rear"
if old_foto.exists():
    # En içteki images klasörünü bul
    images = list(old_foto.rglob("*images*"))
    side = None
    back = None
    for p in old_foto.rglob("*"):
        n = p.name.lower()
        if p.is_dir() and "side" in n and "view" in n and "image" in n:
            side = p
        if p.is_dir() and "back" in n and "view" in n:
            back = p
    print("side dir:", side)
    print("back dir:", back)
    if side:
        safe_merge_dir(side, YANDAN / "side_view")
    if back:
        safe_merge_dir(back, YANDAN / "back_view")
    if not side and not back:
        # junction: yandan -> eski klasör (Windows)
        link = YANDAN / "eski_veri_baglantisi"
        if not link.exists():
            try:
                os.symlink(str(old_foto), str(link), target_is_directory=True)
                print("OK symlink yandan -> 01_cattle_side_rear")
            except Exception:
                try:
                    import subprocess
                    subprocess.run(
                        ["cmd", "/c", "mklink", "/J", str(link), str(old_foto)],
                        check=False,
                    )
                    print("OK junction yandan -> 01_cattle_side_rear")
                except Exception as e:
                    (YANDAN / "OKU.txt").write_text(
                        f"Fotograflar hala burada:\n{old_foto}\nBuraya tasiyabilirsin.\n",
                        encoding="utf-8",
                    )
                    print(f"INFO foto tasinamadi, OKU.txt yazildi: {e}")

(ONDEN / "OKU.txt").write_text(
    "Onden inek fotograflarini bu klasore koy.\n", encoding="utf-8"
)

(ROOT / "README.txt").write_text(
    """FARMEYE DATA
=============

C:\\FarmEyeData\\
  inek_fotograflari\\
    yandan\\     yan / arka foto
    onden\\      on foto
  videolar\\     egitim / dataset videolari
  kayitlar\\
    gunluk_videolar\\YYYY-MM-DD\\   (1 gun)
    veriler\\                      (DB, 1 ay davranis)
      farmeye.db
      people.db
      person_identities.json

Eski 01_ / 02_ klasorleri kaldiysa icerik kopyalandi; bos olanlari silebilirsin.
""",
    encoding="utf-8",
)

print("\n=== Son ust yapi ===")
for p in sorted(ROOT.iterdir()):
    print(" ", "DIR " if p.is_dir() else "FILE", p.name)
print("veriler:", list(VERILER.iterdir()) if VERILER.exists() else None)
print("DONE")
