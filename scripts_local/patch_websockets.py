"""PlatformIO pre-build patch for Arduino-ESP32 NetworkClient deprecation.

WebSockets 2.7.3 calls NetworkClient::flush() before disconnecting. In the
Arduino-ESP32 core used by this project flush() is a deprecated wrapper around
clear(), so use the replacement directly. The dependency lives under .pio and
is regenerated; applying this exact, idempotent patch at build time keeps the
fix durable without modifying downloaded files in source control.
"""

Import("env")  # noqa: F821
from pathlib import Path

OLD = "            client->tcp->flush();"
NEW = """            // CrossPoint patch: Arduino-ESP32 flush() delegates to clear().
            client->tcp->clear();"""
MARKER = "CrossPoint patch: Arduino-ESP32 flush() delegates to clear()."


def ensure_patched(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        return
    if OLD not in text:
        raise RuntimeError(
            f"WebSockets source at {path} no longer matches version 2.7.3; "
            "check whether the compatibility patch is still required."
        )
    path.write_text(text.replace(OLD, NEW, 1), encoding="utf-8")
    print(f"WebSockets NetworkClient compatibility patch applied once: {path}")


project = Path(env.subst("$PROJECT_DIR"))
for name in ("WebSocketsClient.cpp", "WebSocketsServer.cpp"):
    for source in project.glob(f".pio/libdeps/*/WebSockets/src/{name}"):
        ensure_patched(source)
