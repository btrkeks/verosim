"""Small statistical helpers used by harness gates."""

from __future__ import annotations

import math


def ranks(values: list[float]) -> list[float]:
    """Average ranks (ties share the mean rank), 1-based."""
    order = sorted(range(len(values)), key=lambda i: values[i])
    result = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        avg = (i + j) / 2 + 1
        for k in range(i, j + 1):
            result[order[k]] = avg
        i = j + 1
    return result


def pearson(xs: list[float], ys: list[float]) -> float:
    n = len(xs)
    mx = sum(xs) / n
    my = sum(ys) / n
    cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    vx = sum((x - mx) ** 2 for x in xs)
    vy = sum((y - my) ** 2 for y in ys)
    if vx == 0 or vy == 0:
        return float("nan")
    return cov / (vx * vy) ** 0.5


def spearman(xs: list[float], ys: list[float]) -> float:
    return pearson(ranks(xs), ranks(ys))


def spearman_or_none(
    rows: list[dict], x_key: str = "cpp_omr_ned", y_key: str = "oracle_omr_ned"
) -> float | None:
    if len(rows) < 2:
        return None
    rho = spearman([r[x_key] for r in rows], [r[y_key] for r in rows])
    return rho if math.isfinite(rho) else None


def format_rho(value: float | None) -> str:
    return "n/a" if value is None else f"{value:.4f}"
