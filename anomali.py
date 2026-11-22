"""
anomali.py - Pembangkit traffic anomali (HANYA untuk jaringan uji milik sendiri)

Pemakaian (PowerShell, venv aktif):
    python anomali.py <protokol> <skenario> <durasi_detik> [--rate N] [--pi IP]

    protokol : mqtt | http | coap
    skenario : burst | publish | subscribe | topic
    contoh   : python anomali.py mqtt publish 120
               python anomali.py http topic 120 --rate 10

Kebutuhan: pip install paho-mqtt requests
"""
import argparse
import random
import socket
import time
from datetime import datetime

PI_IP = "192.168.10.199"
PORT_HTTP, PORT_MQTT, PORT_COAP = 80, 1883, 5683

# CATATAN: beberapa target di bawah ini (topic MQTT di skenario publish,
# path HTTP di skenario subscribe, resource CoAP "motion") mengasumsikan
# konfigurasi device kalian saat ini. Kalau nanti nama topic/endpoint/resource
# di node ESP32 berubah, cari & sesuaikan nilainya di fungsi aksi_mqtt(),
# aksi_http(), dan aksi_coap() di bawah.

TOPIK_ANEH = ["admin/config", "system/reboot", "rahasia/data", "sensor/lain/suhu", "kontrol/pompa"]
ENDPOINT_ANEH = ["/admin", "/config", "/login", "/api/secret", "/.env", "/api/sensor/lain"]
RESOURCE_ANEH = ["admin", "config", "rahasia", "kontrol", "debug"]


# ---------------- MQTT ----------------
def buat_klien_mqtt(client_id):
    import paho.mqtt.client as mqtt
    try:
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    except AttributeError:          # paho-mqtt versi 1.x
        return mqtt.Client(client_id=client_id)


def aksi_mqtt(skenario):
    if skenario == "burst":
        def kirim(n):
            c = buat_klien_mqtt(f"burst_{n}_{random.randint(0, 99999)}")
            c.connect(PI_IP, PORT_MQTT, 30)
            c.loop_start()
            time.sleep(0.02)
            c.disconnect()
            c.loop_stop()
        return kirim, lambda: None

    c = buat_klien_mqtt(f"anomali_{random.randint(0, 99999)}")
    c.connect(PI_IP, PORT_MQTT, 60)
    c.loop_start()
    if skenario == "publish":
        def kirim(n):
            c.publish("sensor/anomali/suhu", f"{random.uniform(20, 40):.1f}")
    elif skenario == "subscribe":
        def kirim(n):
            c.subscribe(f"sensor/{n % 500}/#")
    else:  # topic
        def kirim(n):
            c.publish(f"{random.choice(TOPIK_ANEH)}/{n % 50}", "x")

    def tutup():
        c.disconnect()
        c.loop_stop()
    return kirim, tutup


# ---------------- HTTP ----------------
def aksi_http(skenario):
    import requests
    base = f"http://{PI_IP}:{PORT_HTTP}"
    if skenario == "burst":      # koneksi TCP baru tiap request
        return (lambda n: requests.get(base + "/", timeout=2,
                                       headers={"Connection": "close"})), (lambda: None)
    s = requests.Session()
    if skenario == "publish":    # POST beruntun lewat satu koneksi
        kirim = lambda n: s.post(base + "/api/sensor/http",
                                 json={"nilai": random.randint(1000, 3000)}, timeout=2)
    elif skenario == "subscribe":  # polling cepat
        kirim = lambda n: s.get(base + "/", timeout=2)
    else:                        # endpoint tidak biasa
        kirim = lambda n: s.get(base + random.choice(ENDPOINT_ANEH), timeout=2)
    return kirim, s.close


# ---------------- CoAP (paket dibuat manual lewat UDP) ----------------
def pesan_coap(kode, path, payload=b"", observe=False, mid=0):
    pesan = bytearray([0x40, kode, (mid >> 8) & 0xFF, mid & 0xFF])  # versi 1, CON, tanpa token
    sebelumnya = 0
    if observe:                                  # opsi Observe (6), nilai 0 = register
        pesan.append((6 - sebelumnya) << 4)
        sebelumnya = 6
    for bagian in path.strip("/").split("/"):    # opsi Uri-Path (11)
        data = bagian.encode()
        pesan.append(((11 - sebelumnya) << 4) | len(data))
        pesan += data
        sebelumnya = 11
    if payload:
        pesan.append(0xFF)
        pesan += payload
    return bytes(pesan)


def aksi_coap(skenario):
    GET, PUT = 0x01, 0x03
    tujuan = (PI_IP, PORT_COAP)
    if skenario == "burst":      # tiap pesan memakai socket (port sumber) baru
        def kirim(n):
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.sendto(pesan_coap(GET, "motion", mid=n & 0xFFFF), tujuan)
            s.close()
        return kirim, lambda: None

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if skenario == "publish":
        def kirim(n):
            s.sendto(pesan_coap(PUT, "motion", b"MOTION_DETECTED", mid=n & 0xFFFF), tujuan)
    elif skenario == "subscribe":
        def kirim(n):
            s.sendto(pesan_coap(GET, "motion", observe=True, mid=n & 0xFFFF), tujuan)
    else:
        def kirim(n):
            s.sendto(pesan_coap(GET, random.choice(RESOURCE_ANEH), mid=n & 0xFFFF), tujuan)
    return kirim, s.close


# ---------------- Pelaksana ----------------
def main():
    global PI_IP
    p = argparse.ArgumentParser(description="Pembangkit traffic anomali (jaringan uji sendiri)")
    p.add_argument("protokol", choices=["mqtt", "http", "coap"])
    p.add_argument("skenario", choices=["burst", "publish", "subscribe", "topic"])
    p.add_argument("durasi", type=int)
    p.add_argument("--rate", type=float, default=20)
    p.add_argument("--pi", default="192.168.10.199")
    p.add_argument("--tunda", type=int, default=0)
    p.add_argument("--run", default="uji")
    p.add_argument("--log", default="anomali_log.csv")
    a = p.parse_args()
    if not a.pi.startswith("192.168.10."):
        raise SystemExit("Ditolak: tujuan harus di 192.168.10.0/24")
    PI_IP = a.pi

    fabrik = {"mqtt": aksi_mqtt, "http": aksi_http, "coap": aksi_coap}[a.protokol]
    kirim, tutup = fabrik(a.skenario)

    if a.tunda:
        time.sleep(a.tunda)
    mulai = datetime.now().astimezone().isoformat(timespec="seconds")
    print(f"Mulai  : {mulai} ({a.protokol}/{a.skenario}, {a.durasi} detik, ~{a.rate:g}/detik)")
    jeda, akhir = 1.0 / a.rate, time.time() + a.durasi
    n = galat = 0
    while time.time() < akhir:
        t0 = time.time()
        try:
            kirim(n)
        except Exception as e:
            galat += 1
            if galat <= 3:
                print("Galat  :", e)
        n += 1
        sisa = jeda - (time.time() - t0)
        if sisa > 0:
            time.sleep(sisa)
    try:
        tutup()
    except Exception:
        pass
    selesai = datetime.now().astimezone().isoformat(timespec="seconds")
    print(f"Selesai: {selesai} {n} aksi, {galat} galat")
    with open(a.log, "a", encoding="utf-8") as f:
        f.write(f"{a.run},{a.protokol},{a.skenario},{mulai},{selesai},{n},{galat}\n")


if __name__ == "__main__":
    main()
