#!/usr/bin/env python3
"""Record ESP-NOW BASE serial output without altering raw.txt."""

import argparse
import math
import sys
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.stderr.write("Missing dependency: python3 -m pip install pyserial\n")
    raise SystemExit(2)


def parse_line(line: str) -> Dict[str, str]:
    """Parse key=value structured logs; unknown/malformed parts are harmless."""
    parts = line.strip().split("|")
    if not parts:
        return {}
    fields = {"kind": parts[0]}
    for part in parts[1:]:
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key] = value
    return fields


def int_or_none(value: Optional[str]) -> Optional[int]:
    try:
        return int(value) if value not in (None, "N/A", "") else None
    except ValueError:
        return None


def float_or_none(value: Optional[str]) -> Optional[float]:
    try:
        return float(value) if value not in (None, "N/A", "") else None
    except ValueError:
        return None


@dataclass
class NodeSummary:
    last_stat: Dict[str, str] = field(default_factory=dict)
    rssi_values: List[int] = field(default_factory=list)
    rtt_values: List[int] = field(default_factory=list)
    outage_started_at: Optional[datetime] = None
    outages_seconds: List[float] = field(default_factory=list)

    def consume(self, fields: Dict[str, str], seen_at: datetime) -> None:
        kind = fields.get("kind")
        if kind == "STAT":
            self.last_stat = fields
        elif kind == "RX":
            rssi = int_or_none(fields.get("rssi"))
            rtt = int_or_none(fields.get("rtt"))
            if rssi is not None:
                self.rssi_values.append(rssi)
            if rtt is not None:
                self.rtt_values.append(rtt)
        elif kind == "EVENT":
            state = fields.get("state")
            if state == "OFFLINE" and self.outage_started_at is None:
                self.outage_started_at = seen_at
            elif state == "ONLINE" and self.outage_started_at is not None:
                self.outages_seconds.append((seen_at - self.outage_started_at).total_seconds())
                self.outage_started_at = None

    def finish(self, ended_at: datetime) -> None:
        if self.outage_started_at is not None:
            self.outages_seconds.append((ended_at - self.outage_started_at).total_seconds())
            self.outage_started_at = None


def choose_port() -> str:
    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found. Connect BASE and use --port /dev/ttyUSB0.")
    print("Serial ports:")
    for index, port in enumerate(ports, start=1):
        description = port.description or "Unknown device"
        print("  {}. {} — {}".format(index, port.device, description))
    while True:
        choice = input("Choose port number: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(ports):
            return ports[int(choice) - 1].device
        print("Invalid selection.")


def live_line(fields: Dict[str, str], raw_line: str, timestamp: datetime) -> str:
    prefix = "[{}]".format(timestamp.strftime("%H:%M:%S.%f")[:-3])
    kind = fields.get("kind")
    node = fields.get("node")
    if kind == "RX" and node:
        bits = ["{}".format(node), "RX", fields.get("type", "?"),
                "seq={}".format(fields.get("seq", "?")),
                "rssi={}".format(fields.get("rssi", "N/A"))]
        if fields.get("rtt") not in (None, "N/A"):
            bits.append("rtt={}ms".format(fields["rtt"]))
        return "{} {}".format(prefix, " ".join(bits))
    if kind == "EVENT" and node:
        return "{} {} EVENT {}".format(prefix, node, fields.get("state", "?"))
    return "{} {}".format(prefix, raw_line.rstrip())


def percentile_95(values: List[int]) -> Optional[int]:
    if not values:
        return None
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * 0.95) - 1)]


def stat_or_calculated(summary: NodeSummary, field: str, fallback: int = 0) -> int:
    value = int_or_none(summary.last_stat.get(field))
    return fallback if value is None else value


def write_node_summary(handle, title: str, summary: NodeSummary) -> None:
    received = stat_or_calculated(summary, "rx")
    lost = stat_or_calculated(summary, "lost")
    duplicates = stat_or_calculated(summary, "dup")
    out_of_order = stat_or_calculated(summary, "ooo")
    total = received + lost
    pdr = 0.0 if total == 0 else received * 100.0 / total

    handle.write("{}\n{}\n\n".format(title, "-" * len(title)))
    handle.write("Packets RX: {}\nEstimated lost: {}\nDuplicates: {}\nOut of order: {}\n\n".format(
        received, lost, duplicates, out_of_order))
    handle.write("PDR:\n{:.2f}%\n\n".format(pdr))

    handle.write("RSSI:\n")
    if summary.rssi_values:
        handle.write("minimum: {} dBm\nmaximum: {} dBm\naverage: {:.1f} dBm\n\n".format(
            min(summary.rssi_values), max(summary.rssi_values),
            sum(summary.rssi_values) / len(summary.rssi_values)))
    else:
        handle.write("N/A (Arduino core callback did not provide RSSI, or no RX records)\n\n")

    if title == "PROBE":
        handle.write("RTT:\n")
        if summary.rtt_values:
            handle.write("minimum: {} ms\nmaximum: {} ms\naverage: {:.1f} ms\np95: {} ms\n\n".format(
                min(summary.rtt_values), max(summary.rtt_values),
                sum(summary.rtt_values) / len(summary.rtt_values), percentile_95(summary.rtt_values)))
        else:
            handle.write("N/A\n\n")

    longest = max(summary.outages_seconds) if summary.outages_seconds else 0.0
    handle.write("Longest communication outage:\n{:.1f} s\n\n".format(longest))


def write_summary(path: Path, started_at: datetime, ended_at: datetime,
                  probe: NodeSummary, gate: NodeSummary) -> None:
    duration = int((ended_at - started_at).total_seconds())
    hours, remainder = divmod(duration, 3600)
    minutes, seconds = divmod(remainder, 60)
    with path.open("w", encoding="utf-8") as handle:
        handle.write("ESP-NOW RANGE TEST\n==================\n\n")
        handle.write("Started: {}\nEnded: {}\nSession duration: {:02d}:{:02d}:{:02d}\n\n".format(
            started_at.strftime("%Y-%m-%d %H:%M:%S"), ended_at.strftime("%Y-%m-%d %H:%M:%S"),
            hours, minutes, seconds))
        write_node_summary(handle, "PROBE", probe)
        write_node_summary(handle, "GATE", gate)


def main() -> int:
    parser = argparse.ArgumentParser(description="Record ESP-NOW BASE structured serial logs.")
    parser.add_argument("--port", help="Serial port, e.g. /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    args = parser.parse_args()

    try:
        port = args.port or choose_port()
    except (RuntimeError, EOFError) as error:
        print("Error: {}".format(error), file=sys.stderr)
        return 2

    project_root = Path(__file__).resolve().parent.parent
    started_at = datetime.now()
    session_dir = project_root / "logs" / started_at.strftime("%Y-%m-%d_%H-%M-%S")
    session_dir.mkdir(parents=True, exist_ok=False)
    raw_path = session_dir / "raw.txt"
    probe_path = session_dir / "probe.txt"
    gate_path = session_dir / "gate.txt"
    summary_path = session_dir / "summary.txt"
    probe_summary = NodeSummary()
    gate_summary = NodeSummary()

    try:
        device = serial.Serial(port, args.baud, timeout=0.5)
    except serial.SerialException as error:
        print("Cannot open {}: {}".format(port, error), file=sys.stderr)
        return 2

    # Discard bytes accumulated while Arduino Serial Monitor/another process had the port.
    # This keeps a new session from starting with stale, partial structured lines.
    device.reset_input_buffer()

    print("Logging {} at {} baud".format(port, args.baud))
    print("Session: {}".format(session_dir))
    print("Press Ctrl+C to finish and create summary.txt.")

    try:
        with raw_path.open("wb") as raw_file, probe_path.open("w", encoding="utf-8") as probe_file, \
                gate_path.open("w", encoding="utf-8") as gate_file:
            while True:
                raw_bytes = device.readline()
                if not raw_bytes:
                    continue
                raw_file.write(raw_bytes)  # Exact bytes from Serial; no terminal timestamp added.
                raw_file.flush()
                line = raw_bytes.decode("utf-8", errors="replace").rstrip("\r\n")
                seen_at = datetime.now()
                fields = parse_line(line)
                node = fields.get("node")
                if node == "PROBE":
                    probe_file.write(line + "\n")
                    probe_file.flush()
                    probe_summary.consume(fields, seen_at)
                elif node == "GATE":
                    gate_file.write(line + "\n")
                    gate_file.flush()
                    gate_summary.consume(fields, seen_at)
                print(live_line(fields, line, seen_at), flush=True)
    except KeyboardInterrupt:
        print("\nStopping logger.")
    finally:
        ended_at = datetime.now()
        device.close()
        probe_summary.finish(ended_at)
        gate_summary.finish(ended_at)
        write_summary(summary_path, started_at, ended_at, probe_summary, gate_summary)
        print("Summary: {}".format(summary_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
