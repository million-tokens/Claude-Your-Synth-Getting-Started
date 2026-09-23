"""Tests for the workshop status line. Run: python3 -m unittest discover -s claude-config"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import statusline  # noqa: E402


def session(tokens_in: int = 12400, tokens_out: int = 1800, cost: float = 0.42, over: bool = False) -> dict:
    return {
        "workspace": {"current_dir": "/home/me/Claude-Your-Synth-Getting-Started"},
        "context_window": {"total_input_tokens": tokens_in, "total_output_tokens": tokens_out},
        "cost": {"total_cost_usd": cost},
        "exceeds_200k_tokens": over,
    }


class Rendering(unittest.TestCase):
    def test_prints_the_example_line(self):
        self.assertEqual(
            statusline.render(session(), "my-synth"),
            "Claude-Your-Synth-Getting-Started (my-synth) in 12.4k out 1.8k ctx 14.2k $0.42",
        )

    def test_leaves_out_the_branch_outside_git(self):
        self.assertTrue(statusline.render(session(), "").startswith("Claude-Your-Synth-Getting-Started in "))

    def test_warns_in_red_past_200k(self):
        line = statusline.render(session(231600, 9200, 6.8, over=True), "my-synth")
        self.assertIn(f"{statusline.RED}⚠ ctx 240.8k{statusline.RESET}", line)

    def test_shows_zeros_before_the_first_response(self):
        data = {"workspace": {"current_dir": "/tmp/x"}, "context_window": {"current_usage": None}}
        self.assertEqual(statusline.render(data, ""), "x in 0 out 0 ctx 0 $0.00")

    def test_small_numbers_stay_whole(self):
        self.assertEqual(statusline.short(950), "950")
        self.assertEqual(statusline.short(1000), "1.0k")


if __name__ == "__main__":
    unittest.main()
