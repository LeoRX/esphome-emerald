"""Protocol-level regression vectors for Emerald 30-second power frames."""

import importlib.util
from pathlib import Path
import unittest

MODULE = Path(__file__).parents[1] / "tools" / "emerald_protocol.py"
spec = importlib.util.spec_from_file_location("emerald_protocol", MODULE)
assert spec is not None and spec.loader is not None
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)


class EmeraldProtocolVectorsTest(unittest.TestCase):
    def test_valid_frame_decodes_power(self):
        frame = bytes((0x00, 0x01, 0x02, 0x0A, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C))
        self.assertEqual(protocol.decode_30s_power_frame(frame, pulses_per_kwh=1000).watts, 840.0)

    def test_short_frame_is_rejected(self):
        with self.assertRaises(ValueError):
            protocol.decode_30s_power_frame(b"\x00" * 10, pulses_per_kwh=1000)

    def test_unknown_header_is_rejected(self):
        frame = bytes((0xFF, 0x01, 0x02, 0x0A, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C))
        with self.assertRaises(ValueError):
            protocol.decode_30s_power_frame(frame, pulses_per_kwh=1000)

    def test_invalid_calibration_is_rejected(self):
        frame = bytes((0x00, 0x01, 0x02, 0x0A, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C))
        with self.assertRaises(ValueError):
            protocol.decode_30s_power_frame(frame, pulses_per_kwh=0)


if __name__ == "__main__":
    unittest.main()
