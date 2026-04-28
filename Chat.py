import os
import sqlite3
from datetime import datetime
from flask import Flask, request, jsonify, render_template, send_from_directory

app = Flask(__name__)

UPLOAD_FOLDER = os.path.expanduser("upload")
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

app.config["UPLOAD_FOLDER"] = UPLOAD_FOLDER
app.config["MAX_CONTENT_LENGTH"] = 10 * 1024 * 1024

db_path = os.path.expanduser("chat.db")

conn = sqlite3.connect(db_path)
c = conn.cursor()

c.execute("""
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender TEXT,
    message TEXT,
    date TEXT,
    attachments TEXT
)
""")

# dodanie kolumny jeśli nie istnieje
try:
    c.execute("ALTER TABLE messages ADD COLUMN attachments TEXT")
except:
    pass

conn.commit()
conn.close()


def add_message(sender, text, files):
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    attachments = ";".join(files) if files else ""

    c.execute(
        "INSERT INTO messages (sender, message, date, attachments) VALUES (?, ?, ?, ?)",
        (sender, text, now, attachments)
    )

    conn.commit()
    conn.close()


def get_all_messages():
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    c.execute("SELECT id, sender, message, date, attachments FROM messages ORDER BY id ASC")
    rows = c.fetchall()

    conn.close()

    return [
        {
            "id": r[0],
            "sender": r[1],
            "message": r[2],
            "date": r[3],
            "attachments": [a for a in (r[4] or "").split(";") if a]
        }
        for r in rows
    ]


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/send", methods=["POST"])
def send():
    user = request.form.get("user", "Anon")
    text = request.form.get("text", "")
    files = request.files.getlist("files")

    saved_files = []

    if len(files) > 5:
        return jsonify({"error": "max 5 files"}), 400

    for f in files:
        if not f.filename:
            continue

        filename = f"{datetime.now().timestamp()}_{f.filename}"
        path = os.path.join(app.config["UPLOAD_FOLDER"], filename)
        f.save(path)

        saved_files.append(filename)

    if text.strip() or saved_files:
        add_message(user, text, saved_files)

    return jsonify({"status": "ok"})


@app.route("/upload/<filename>")
def uploaded_file(filename):
    return send_from_directory(app.config["UPLOAD_FOLDER"], filename)


@app.route("/messages")
def messages():
    return jsonify(get_all_messages())


@app.route("/count")
def count():
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    c.execute("SELECT COUNT(*) FROM messages")
    count = c.fetchone()[0]
    conn.close()
    return jsonify({"count": count})


@app.route("/delete/<int:msg_id>", methods=["POST"])
def delete_message(msg_id):
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    c.execute("SELECT attachments FROM messages WHERE id = ?", (msg_id,))
    row = c.fetchone()

    if row and row[0]:
        for f in row[0].split(";"):
            path = os.path.join(app.config["UPLOAD_FOLDER"], f)
            if os.path.exists(path):
                os.remove(path)

    c.execute("DELETE FROM messages WHERE id = ?", (msg_id,))
    conn.commit()
    conn.close()

    return jsonify({"status": "deleted"})


@app.route("/clear", methods=["POST"])
def clear():
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    c.execute("SELECT attachments FROM messages")
    rows = c.fetchall()

    for r in rows:
        if r[0]:
            for f in r[0].split(";"):
                path = os.path.join(app.config["UPLOAD_FOLDER"], f)
                if os.path.exists(path):
                    os.remove(path)

    c.execute("DELETE FROM messages")
    conn.commit()
    conn.close()

    return jsonify({"status": "cleared"})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)