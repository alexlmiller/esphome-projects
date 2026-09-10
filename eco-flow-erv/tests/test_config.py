"""Validate package overrides and reject unsafe prototype restore/framing options.

Run with the repo's ESPHome virtualenv: python eco-flow-erv/tests/test_config.py
Fixtures use only example secrets and temporary YAML; no hardware access.
"""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

PACKAGE = Path(__file__).resolve().parents[1]


class ConfigurationTests(unittest.TestCase):
    def check_config(self, extra="", success=True, expected=""):
        with tempfile.TemporaryDirectory(prefix="erv-config-test-") as directory:
            target = Path(directory)
            shutil.copyfile(PACKAGE.parent / "secrets.example.yaml", target / "secrets.yaml")
            config = (
                f"packages:\n  base: !include {PACKAGE / 'eco-flow-erv.yaml'}\n"
                "external_components:\n"
                f"  source: {PACKAGE / 'components'}\n"
                "  components: [eco_flow_erv]\n" + extra
            )
            path = target / "fixture.yaml"
            path.write_text(config)
            result = subprocess.run(
                [sys.executable, "-m", "esphome", "config", str(path)],
                capture_output=True, text=True, timeout=60,
            )
            output = result.stdout + result.stderr
            self.assertEqual(result.returncode == 0, success, output)
            if expected:
                self.assertIn(expected, output)
            return output

    def test_defaults(self):
        output = self.check_config()
        self.assertRegex(output, r"tx_pin:\n\s+number: (?:1|GPIO1)\n\s+inverted: false\n")
        self.assertRegex(output, r"rx_pin:\n\s+number: (?:2|GPIO2)\n\s+inverted: true\n")

    def test_recovery_opt_in(self):
        self.check_config("substitutions:\n  erv_enable_recovery: 'true'\n")

    def test_verbose_logging(self):
        self.check_config("substitutions:\n  erv_log_level: VERBOSE\n")

    def test_info_logging(self):
        self.check_config("substitutions:\n  erv_log_level: INFO\n")

    def test_raw_logging(self):
        self.check_config("substitutions:\n  erv_log_level: VERY_VERBOSE\n")

    def test_network_only_logging(self):
        self.check_config("substitutions:\n  erv_log_baud: '0'\n")

    def test_fast_diagnostics_rejected(self):
        self.check_config("substitutions:\n  erv_diagnostics_interval: 1ms\n", False)

    def test_no_rx_pin(self):
        self.check_config("uart:\n  rx_pin: !remove\n")

    def test_inversion_overrides(self):
        output = self.check_config("substitutions:\n  erv_tx_inverted: 'true'\n  erv_rx_inverted: 'false'\n")
        self.assertRegex(output, r"tx_pin:\n\s+number: (?:1|GPIO1)\n\s+inverted: true\n")
        self.assertRegex(output, r"rx_pin:\n\s+number: (?:2|GPIO2)\n\s+inverted: false\n")

    def test_on_restore_rejected(self):
        self.check_config("fan:\n  - id: !extend erv\n    restore_mode: ALWAYS_ON\n", False, "NO_RESTORE")

    def test_auto_arm_rejected(self):
        self.check_config(
            "fan:\n  - id: !extend erv\n    control_enabled:\n      restore_mode: ALWAYS_ON\n",
            False, "ALWAYS_OFF",
        )

    def test_gate_inversion_rejected(self):
        self.check_config(
            "fan:\n  - id: !extend erv\n    control_enabled:\n      inverted: true\n",
            False, "Inverted is not supported",
        )

    def test_parity_rejected(self):
        self.check_config("uart:\n  parity: NONE\n", False, "parity EVEN")

    def test_stop_bits_rejected(self):
        self.check_config("uart:\n  stop_bits: 1\n", False, "2 stop bits")

    def test_short_cadence_rejected(self):
        self.check_config("substitutions:\n  erv_frame_interval: 10ms\n", False)


if __name__ == "__main__":
    unittest.main(verbosity=2)
