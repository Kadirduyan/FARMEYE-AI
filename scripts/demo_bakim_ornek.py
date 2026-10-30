import sqlite3
import os

root = r"C:\FarmEyeData"
os.makedirs(root, exist_ok=True)
os.makedirs(os.path.join(root, "videos", "2026-07-21"), exist_ok=True)
db = os.path.join(root, "farmeye_demo_ornek.db")
if os.path.exists(db):
    os.remove(db)

c = sqlite3.connect(db)
c.executescript(
    """
CREATE TABLE animals(id INTEGER PRIMARY KEY, tag TEXT, name TEXT);
CREATE TABLE behavior_events(
  id INTEGER PRIMARY KEY, animal_id INT, day TEXT, ts TEXT, event_type TEXT, note TEXT);
CREATE TABLE behavior_daily(
  animal_id INT, day TEXT, water_sec INT, eat_sec INT, sleep_sec INT,
  fall_count INT, limp_score_max REAL, PRIMARY KEY(animal_id, day));
"""
)
c.execute("INSERT INTO animals VALUES (3, 'INEK-003', 'Boncuk')")
rows = [
    (3, "2026-07-21", "2026-07-21T08:12:03", "drink_start", "Suluğa gitti"),
    (3, "2026-07-21", "2026-07-21T08:14:41", "drink_end", "158 sn su"),
    (3, "2026-07-21", "2026-07-21T09:05:10", "eat_start", "Yemlik"),
    (3, "2026-07-21", "2026-07-21T09:22:00", "eat_end", "1010 sn yem"),
    (3, "2026-07-21", "2026-07-21T11:40:00", "lie_start", "Yattı"),
    (3, "2026-07-21", "2026-07-21T14:10:00", "lie_end", "Uyku/yatma bitti"),
    (3, "2026-07-21", "2026-07-21T16:32:05", "limp_start", "Topallık başladı"),
]
c.executemany(
    "INSERT INTO behavior_events(animal_id, day, ts, event_type, note) VALUES (?,?,?,?,?)",
    rows,
)
c.execute(
    "INSERT INTO behavior_daily VALUES (3, '2026-07-21', 158, 1010, 9000, 0, 0.87)"
)
c.commit()

print("Demo DB:", db)
print("--- BAKIM EKRANI: İnek 003 Boncuk / 21 Temmuz ---")
for r in c.execute(
    "SELECT ts, event_type, note FROM behavior_events WHERE animal_id=3 ORDER BY ts"
):
    print(f"  {r[0][11:19]}  {r[1]:12}  {r[2]}")
d = c.execute(
    "SELECT water_sec, eat_sec, sleep_sec, limp_score_max FROM behavior_daily WHERE animal_id=3"
).fetchone()
print(
    f"GÜNLÜK ÖZET: su={d[0]}s  yem={d[1]}s  yatma={d[2]}s  topallık_max={d[3]}"
)
print("Video (sadece bugun): C:/FarmEyeData/videos/2026-07-21/")
print("Yarin: video klasoru SILINIR; bu davranis satirlari 30 gun kalir.")
