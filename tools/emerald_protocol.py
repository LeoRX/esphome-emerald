"""Pure-Python reference decoder for the Emerald 30-second notification frame.

This intentionally mirrors only the verified packet contract used by the ESPHome
component, making the safety-critical bounds and calibration maths testable without
an ESP32 or a live meter.
"""

from dataclasses import dataclass

POWER_FRAME_HEADER = bytes((0x00, 0x01, 0x02, 0x0A, 0x06))
POWER_FRAME_LENGTH = 11
INTERVAL_SECONDS = 30


@dataclass(frozen=True)
class PowerMeasurement:
    pulses: int
    watts: float
    interval_kwh: float


def decode_30s_power_frame(frame: bytes, pulses_per_kwh: float) -> PowerMeasurement:
    """Decode one verified Emerald 30-second power notification safely."""
    if pulses_per_kwh <= 0:
        raise ValueError("pulses_per_kwh must be positive")
    if len(frame) != POWER_FRAME_LENGTH:
        raise ValueError(f"expected {POWER_FRAME_LENGTH}-byte power frame, got {len(frame)}")
    if frame[: len(POWER_FRAME_HEADER)] != POWER_FRAME_HEADER:
        raise ValueError("unexpected Emerald notification header")

    pulses = int.from_bytes(frame[9:11], byteorder="big")
    watts = pulses * INTERVAL_SECONDS * 1000.0 / pulses_per_kwh
    return PowerMeasurement(pulses=pulses, watts=watts, interval_kwh=pulses / pulses_per_kwh)
