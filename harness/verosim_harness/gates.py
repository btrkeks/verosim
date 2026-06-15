"""First-class summaries for harness gate policies."""

from __future__ import annotations

from dataclasses import dataclass


def threshold_status(value: float | None, threshold: float) -> str:
    return "PASS" if value is not None and value >= threshold else "FAIL"


@dataclass(frozen=True)
class RecordCompleteness:
    expected: int
    emitted: int

    @property
    def missing(self) -> int:
        return self.expected - self.emitted

    @property
    def ok(self) -> bool:
        return self.missing == 0


@dataclass(frozen=True)
class CleanFailureAudit:
    completeness: RecordCompleteness
    recovered_loads: list[dict]
    missing_errors: list[dict]

    @property
    def ok(self) -> bool:
        return self.completeness.ok and not self.missing_errors


def clean_failure_audit(expected: int, records: list[dict]) -> CleanFailureAudit:
    return CleanFailureAudit(
        completeness=RecordCompleteness(expected=expected, emitted=len(records)),
        recovered_loads=[r for r in records if r.get("ok")],
        missing_errors=[r for r in records if not r.get("ok") and not r.get("errors")],
    )
