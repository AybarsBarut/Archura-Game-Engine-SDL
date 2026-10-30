"""Configuration failures must exit before starting a native server listener."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument("--server", required=True, type=Path)
args = parser.parse_args()
server = args.server.resolve(strict=True)


class ServerConfigProcessContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="archura-server-config-process-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.config = self.root / "server.json"

    def run_server(self, arguments):
        return subprocess.run([str(server), *arguments], cwd=self.root, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=3,
                              creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)

    def rejects(self, arguments):
        result = self.run_server(arguments)
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("Invalid server command line or configuration", result.stdout)
        self.assertNotIn("NETWORK_READY", result.stdout)
        self.assertNotIn("Starting network listener", result.stdout)

    def test_missing_explicit_file_rejects(self):
        self.rejects(["--config", str(self.config)])

    def test_malformed_and_unsafe_files_reject(self):
        for text in ['{"port":27015,"port":27016}', '{"port":"27015"}',
                     '{"verbose_logging":1}', '{"client_timeout":1e30}',
                     '{"connection_timeout":0.09}', '{"connection_timeout":121}',
                     '{"client_timeout":601}', '{"connection_timeout":20,"client_timeout":19}',
                     '{"port":27015,}', '{"port":27015} trailing}']:
            with self.subTest(config=text):
                self.config.write_text(text, encoding="utf-8")
                self.rejects(["--config", str(self.config)])

    def test_invalid_command_lines_reject(self):
        for arguments in [["--port"], ["--config"], ["--name"], ["--unknown"],
                          ["--port", "27015suffix"], ["--tickrate", "128.0"],
                          ["--maxplayers", "99999999999999999999"], ["--port", "--verbose"]]:
            with self.subTest(arguments=arguments):
                self.rejects(arguments)

    def test_duplicate_config_rejects(self):
        self.config.write_text("{}", encoding="utf-8")
        self.rejects(["--config", str(self.config), "--config", str(self.config)])

    def test_help_and_valid_override_order_do_not_start_listener(self):
        self.config.write_text(json.dumps({"connection_timeout": 0.2, "client_timeout": 0.7}),
                               encoding="utf-8")
        for arguments in [["--help"], ["--port", "28016", "--config", str(self.config), "--help"],
                          ["--config", str(self.config), "--port", "28016", "--help"]]:
            with self.subTest(arguments=arguments):
                result = self.run_server(arguments)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertIn("Usage: ArchuraServer", result.stdout)
                self.assertNotIn("Starting network listener", result.stdout)
                self.assertNotIn("NETWORK_READY", result.stdout)


if __name__ == "__main__":
    unittest.main(argv=[__file__])
