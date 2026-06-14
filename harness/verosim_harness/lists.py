"""Shared comment/blank-aware corpus list readers."""

from __future__ import annotations

from pathlib import Path


def read_list(path: Path) -> list[str]:
    rows: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        row = line.strip()
        if row and not row.startswith("#"):
            rows.append(row)
    return rows


def read_pairs(path: Path) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    for row in read_list(path):
        pred, gt = row.split("\t")
        pairs.append((pred, gt))
    return pairs
