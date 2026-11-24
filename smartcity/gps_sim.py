#!/usr/bin/env python3
"""GPS simulator: streams NMEA RMC sentences to the board over TCP,
like a GPS module would over UART.

Usage:
  python3 gps_sim.py                 # walk through the Gothenburg points
  python3 gps_sim.py --no-fix        # send "no fix" (status V) sentences
  python3 gps_sim.py --bad-checksum  # send sentences with a wrong checksum
  python3 gps_sim.py --host 192.168.137.42 --port 5001 --interval 5
"""
import argparse
import socket
import time

# Bin locations in Gothenburg (latitude, longitude in decimal degrees).
POINTS = [
    (57.686900, 11.942600),  # Slottsskogen
    (57.697177, 11.979191),  # Götaplatsen (real waste basket from OSM)
    (57.708900, 11.973300),  # Central Station
    (57.695300, 11.992200),  # Liseberg
]


def checksum(body):
    value = 0
    for ch in body:
        value ^= ord(ch)
    return f"{value:02X}"


def nmea_coordinate(value, is_latitude):
    """57.6869 -> ("5741.21400", "N"); 11.9426 -> ("01156.55600", "E")."""
    if is_latitude:
        hemisphere = "N" if value >= 0 else "S"
    else:
        hemisphere = "E" if value >= 0 else "W"
    value = abs(value)
    degrees = int(value)
    minutes = (value - degrees) * 60
    width = 2 if is_latitude else 3
    return f"{degrees:0{width}d}{minutes:08.5f}", hemisphere


def rmc_sentence(lat, lon, fix=True, bad_checksum=False):
    now = time.gmtime()
    lat_text, lat_hemi = nmea_coordinate(lat, True)
    lon_text, lon_hemi = nmea_coordinate(lon, False)
    body = (f"GPRMC,{time.strftime('%H%M%S', now)}.00,{'A' if fix else 'V'},"
            f"{lat_text},{lat_hemi},{lon_text},{lon_hemi},0.0,0.0,"
            f"{time.strftime('%d%m%y', now)},,,A")
    check = "00" if bad_checksum else checksum(body)
    return f"${body}*{check}\r\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="192.168.137.42", help="board IP address")
    parser.add_argument("--port", type=int, default=5001, help="board NMEA port")
    parser.add_argument("--interval", type=float, default=5.0,
                        help="seconds at each point (one sentence per second)")
    parser.add_argument("--no-fix", action="store_true", help="send status V (no fix)")
    parser.add_argument("--bad-checksum", action="store_true", help="send wrong checksums")
    args = parser.parse_args()

    print(f"Connecting to {args.host}:{args.port} ...")
    with socket.create_connection((args.host, args.port), timeout=5) as sock:
        print("Connected. Press Ctrl+C to stop.")
        while True:
            for lat, lon in POINTS:
                for _ in range(max(1, int(args.interval))):
                    line = rmc_sentence(lat, lon, fix=not args.no_fix,
                                        bad_checksum=args.bad_checksum)
                    print(line.strip())
                    sock.sendall(line.encode("ascii"))
                    time.sleep(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")