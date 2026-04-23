from __future__ import annotations

import argparse
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def merge_images(parts: list[tuple[int, Path]], output_path: Path) -> None:
    highest_end = 0
    payloads: list[tuple[int, bytes]] = []
    for offset, path in parts:
        data = path.read_bytes()
        payloads.append((offset, data))
        highest_end = max(highest_end, offset + len(data))

    merged = bytearray(b"\xFF" * highest_end)
    for offset, data in payloads:
        merged[offset : offset + len(data)] = data

    output_path.write_bytes(merged)


def render_template(template_path: Path, replacements: dict[str, str], output_path: Path) -> None:
    content = template_path.read_text(encoding="utf-8")
    for key, value in replacements.items():
        content = content.replace(key, value)
    output_path.write_text(content, encoding="utf-8")


def write_release_metadata(
    release_dir: Path,
    site_dir: Path,
    tag: str,
    git_commit: str,
    board_target: str,
    build_date_utc: str,
) -> None:
    semver = tag[1:] if tag.startswith("v") else tag
    metadata = {
        "tag": tag,
        "version": semver,
        "git_commit": git_commit,
        "board_target": board_target,
        "build_date_utc": build_date_utc,
    }
    metadata_json = json.dumps(metadata, indent=2, sort_keys=True) + "\n"
    (release_dir / "release-metadata.json").write_text(metadata_json, encoding="utf-8")
    (site_dir / "release-metadata.json").write_text(metadata_json, encoding="utf-8")


def build_release_package(
    build_dir: Path,
    release_dir: Path,
    site_dir: Path,
    tag: str,
    repository: str,
    git_commit: str,
    board_target: str,
    build_date_utc: str,
) -> None:
    release_dir.mkdir(parents=True, exist_ok=True)
    site_dir.mkdir(parents=True, exist_ok=True)

    app_image_path = build_dir / "update.bin"
    if not app_image_path.exists():
        app_image_path = build_dir / "firmware.bin"

    update_output_path = release_dir / "update.bin"
    shutil.copyfile(app_image_path, update_output_path)

    firmware_output_path = release_dir / "firmware.bin"
    merge_images(
        [
            (0x1000, build_dir / "bootloader.bin"),
            (0x11000, build_dir / "partitions.bin"),
            (0x18000, build_dir / "ota_data_initial.bin"),
            (0x20000, update_output_path),
        ],
        firmware_output_path,
    )

    installer_dir = REPO_ROOT / "web-installer"
    shutil.copyfile(installer_dir / "styles.css", site_dir / "styles.css")
    (site_dir / ".nojekyll").write_text("", encoding="utf-8")

    zip_url = f"https://github.com/{repository}/releases/download/{tag}/firmware.zip"
    replacements = {
        "__TAG__": tag,
        "__ZIP_URL__": zip_url,
    }
    render_template(installer_dir / "index.html", replacements, site_dir / "index.html")
    render_template(installer_dir / "manifest.template.json", replacements, site_dir / "manifest.json")
    write_release_metadata(release_dir, site_dir, tag, git_commit, board_target, build_date_utc)


def main() -> None:
    parser = argparse.ArgumentParser(description="Create release and bundled web-installer artifacts for LED SmartClock.")
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--release-dir", required=True, type=Path)
    parser.add_argument("--site-dir", required=True, type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--git-commit", default="unknown")
    parser.add_argument("--board-target", default="unknown")
    parser.add_argument(
        "--build-date-utc",
        default=datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    )
    args = parser.parse_args()

    build_release_package(
        args.build_dir,
        args.release_dir,
        args.site_dir,
        args.tag,
        args.repository,
        args.git_commit,
        args.board_target,
        args.build_date_utc,
    )


if __name__ == "__main__":
    main()
