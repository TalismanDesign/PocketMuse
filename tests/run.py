"""Build and run regression tests against the SDK headers without hardware."""

import os
from pathlib import Path
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    sdk = Path(os.environ.get(
        "SDK_ROOT", root.parent / "PocketMage_PDA/Code/PocketMageOS/lib/PocketMage_SDK"
    )).resolve()
    shared = ["tests/support/fake_sd.cpp", "src/playlist.cpp"]
    tests = {
        "playlist": ["tests/unit/playlist_test.cpp", *shared],
        "player": ["tests/unit/player_test.cpp", *shared, "src/ui_player.cpp",
                   "src/muse.cpp", "src/library.cpp"],
        "audio-playback": [str(sdk / "tools/pm/tests/native/audio_playback_test.cpp")],
        "idle-timer": [str(sdk / "tools/pm/tests/native/idle_timer_test.cpp")],
    }
    with tempfile.TemporaryDirectory(prefix="pocketmuse-tests-") as build:
        for name, sources in tests.items():
            binary = str(Path(build) / name)
            subprocess.run([
                os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-Iinclude", "-Itests/support",
                "-idirafter", str(sdk), *sources, "-o", binary,
            ], cwd=root, check=True)
            subprocess.run([binary], check=True)


if __name__ == "__main__":
    main()
