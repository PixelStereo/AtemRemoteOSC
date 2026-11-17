"""Refresh the README download links and the VERSION file from the latest complete release."""

import json
from pathlib import Path
import re
import sys

# Release files carry the tag, as in NyxMap and ISFLab: AtemRemoteOSC-macos-v5.0.6.zip
ASSETS = {
    "macOS": ("AtemRemoteOSC-macos-{tag}.zip", "apple", "34CC34"),
    "Windows": ("AtemRemoteOSC-windows-{tag}-Setup.exe", "windows", "CCCC34"),
}
START = "<!-- release-downloads:start -->"
END = "<!-- release-downloads:end -->"


def download_block(releases, repository):
    base = f"https://github.com/{repository}/releases"
    for release in sorted(releases, key=lambda item: item.get("published_at") or "", reverse=True):
        tag = release["tag_name"]
        version = re.fullmatch(r"v([0-9]+\.[0-9]+\.[0-9]+)", tag)
        if release.get("draft") or not release.get("published_at") or not version:
            continue
        assets = {item["name"]: item["browser_download_url"] for item in release.get("assets", [])
                  if item.get("state") == "uploaded" and item.get("size", 0) > 0}
        urls = {}
        for platform, (pattern, _, _) in ASSETS.items():
            name = pattern.format(tag=tag)
            if name not in assets:
                break
            urls[platform] = f"{base}/download/{tag}/{name}"
        if len(urls) != len(ASSETS):
            continue  # an older release, or one still being uploaded
        badges = "\n".join(
            f"[![{platform}](https://img.shields.io/badge/{platform}-Download-{color}?style=for-the-badge"
            f"&logo={logo}&logoColor=white)]({urls[platform]})"
            for platform, (_, logo, color) in ASSETS.items())
        return f"**LATEST VERSION · [{tag}]({base}/tag/{tag})**\n\n{badges}", version.group(1)
    return f"Downloads appear here after the first release: see the [releases page]({base}).", None


def main():
    path = Path("README.md")
    source = path.read_text(encoding="utf-8")
    if source.count(START) != 1 or source.count(END) != 1:
        raise ValueError("Expected one pair of README download markers")
    prefix, _, tail = source.partition(START)
    _, marker, suffix = tail.partition(END)
    if not marker:
        raise ValueError("README download markers are out of order")
    releases = [release for page in json.load(sys.stdin) for release in page]
    block, version = download_block(releases, sys.argv[1])
    updated = f"{prefix}{START}\n{block}\n{END}{suffix}"
    if updated != source:
        path.write_text(updated, encoding="utf-8")
    if version:
        Path("VERSION").write_text(version + "\n", encoding="utf-8")


main()
