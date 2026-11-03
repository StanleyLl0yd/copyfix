#!/usr/bin/env python3
"""Verify the preserved bootstrap Git bundle without checking out historical code."""
from __future__ import annotations

from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT / "archive" / "CopyFix-bootstrap-history.bundle"
PATTERNS = (
    re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(rb"github_pat_[A-Za-z0-9_]{20,}|gh[pousr]_[A-Za-z0-9]{25,}"),
    re.compile(rb"AKIA[0-9A-Z]{16}"),
)


def git(*args: str) -> bytes:
    result = subprocess.run(
        ["git", *args], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    return result.stdout


def main() -> None:
    if not BUNDLE.is_file():
        raise RuntimeError("Preserved bootstrap bundle missing")
    with tempfile.TemporaryDirectory(prefix="copyfix-bundle-audit-") as tmp:
        checkout = str(Path(tmp) / "history")
        git("clone", "--quiet", "--no-checkout", str(BUNDLE), checkout)
        git("-C", checkout, "fsck", "--full", "--strict", "--no-reflogs")
        objects = {
            row.split(b" ", 1)[0].decode("ascii")
            for row in git("-C", checkout, "rev-list", "--objects", "--all").splitlines()
        }
        if not objects:
            raise RuntimeError("Bootstrap bundle contains no reachable objects")
        blobs = 0
        for object_id in sorted(objects):
            if git("-C", checkout, "cat-file", "-t", object_id).strip() != b"blob":
                continue
            blobs += 1
            data = git("-C", checkout, "cat-file", "blob", object_id)
            if any(pattern.search(data) for pattern in PATTERNS):
                raise RuntimeError("Credential-like pattern in archived Git blob")
        if not blobs:
            raise RuntimeError("Bootstrap bundle contains no accessible blobs")
        print(f"PASS archived Git bundle integrity: {len(objects)} objects, {blobs} blobs screened")


if __name__ == "__main__":
    main()
