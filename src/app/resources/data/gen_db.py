#!/usr/bin/env python3
"""从 poems.json 生成 poems.db SQLite 数据库"""

import json
import os
import sqlite3
import sys

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    json_path = os.path.join(script_dir, "poems.json")
    db_path = os.path.join(script_dir, "poems.db")

    if not os.path.exists(json_path):
        print(f"Error: {json_path} not found")
        sys.exit(1)

    with open(json_path, "r", encoding="utf-8") as f:
        poems = json.load(f)

    if os.path.exists(db_path):
        os.remove(db_path)

    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE poems (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sentence TEXT NOT NULL,
            source TEXT NOT NULL,
            commentary TEXT,
            tag TEXT,
            dynasty TEXT
        )
    """)

    for p in poems:
        c.execute(
            "INSERT INTO poems (sentence, source, commentary, tag, dynasty) VALUES (?, ?, ?, ?, ?)",
            (p.get("sentence", ""), p.get("source", ""), p.get("commentary", ""),
             p.get("tag", ""), p.get("dynasty", "")),
        )

    conn.commit()
    count = c.execute("SELECT count(*) FROM poems").fetchone()[0]
    conn.close()

    print(f"Generated {db_path} ({count} poems, {os.path.getsize(db_path)} bytes)")

if __name__ == "__main__":
    main()
