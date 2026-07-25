# Evaluation profiles

Evaluation profiles define which canonical notation dimensions participate in
an OMR-NED comparison. They are external JSON files: VeroSim owns the feature
taxonomy, validation, projection, and audit behavior, while each benchmark
owns and versions the profile that describes its ground-truth contract.

Use a profile when prediction and ground truth intentionally encode different
notation scopes. VeroSim still parses both complete source documents. It then
projects both extracted `SymScore` values symmetrically before notation-size
calculation, matching, edit-path selection, edit categorization, and OMR-NED.
It never rewrites MusicXML, MEI, Humdrum/kern, or other source data.

## CLI contract

Pass the same profile to any comparison interface:

```sh
# One mixed-format pair
build/verosim prediction.musicxml ground-truth.krn \
  --evaluation-profile profiles/benchmark-v1.json

# Sequential and parallel file-pair lists
build/verosim --pairs pairs.tsv \
  --evaluation-profile profiles/benchmark-v1.json
build/verosim --batch pairs.tsv --jobs 4 \
  --evaluation-profile profiles/benchmark-v1.json

# Mixed-format score strings in JSONL
build/verosim --batch-jsonl validation.jsonl \
  --pred-format musicxml --gt-format kern \
  --summary-json evaluation-summary.json \
  --evaluation-profile profiles/benchmark-v1.json

# HTML or raw-SVG visualization
build/verosim --visualize prediction.musicxml ground-truth.krn \
  --out report.html \
  --evaluation-profile profiles/benchmark-v1.json
build/verosim --visualize prediction.musicxml ground-truth.krn \
  --out-dir report-svg --output-format svg \
  --evaluation-profile profiles/benchmark-v1.json
```

A profile is loaded and validated once, before any score input is processed.
Invalid profiles exit with status 2 and an actionable path-qualified diagnostic.
`--evaluation-profile` cannot be combined with an explicit `--mode` or
`--layout`, even when the explicit value is the normal default. The profile is
the sole scope authority and automatically enables extraction of all canonical
features currently supported by schema v1 (`experimental` plus system breaks).
Options unrelated to scope, including `--ops`, `--note-position`, and
`--typed-space-handling`, remain available.

With no profile, VeroSim follows the existing active/experimental and layout
behavior exactly and emits no profile or removal-audit fields.

## Schema v1

The top-level object has exactly four fields:

```json
{
  "schema": "verosim.evaluation-profile",
  "schema_version": 1,
  "id": "my-benchmark/notation-scope-v1",
  "features": {
    "note.pitch": "scored",
    "note.accidental": "scored",
    "note.tie": "ignored",
    "note.notehead": "scored",
    "note.dot": "scored",
    "note.beam": "scored",
    "note.tuplet": "scored",
    "note.tuplet_info": "scored",
    "note.grace": "scored",
    "note.grace_slash": "scored",
    "note.gap": "scored",
    "note.articulation": "ignored",
    "note.expression": "ignored",
    "extra.arpeggio": "ignored",
    "extra.barline": "scored",
    "extra.clef": "scored",
    "extra.hairpin.crescendo": "ignored",
    "extra.hairpin.diminuendo": "ignored",
    "extra.dynamic": "ignored",
    "extra.keysig": "scored",
    "extra.ottava": "ignored",
    "extra.repeat": "scored",
    "extra.slur": "ignored",
    "extra.timesig": "scored",
    "layout.system_break": "ignored",
    "note.ornament": "unsupported",
    "note.style": "unsupported",
    "note.lyric": "unsupported",
    "note.fingering": "unsupported",
    "extra.text": "unsupported",
    "extra.pedal": "unsupported",
    "layout.page_break": "unsupported"
  }
}
```

The example illustrates the schema; it is not a Transcoda or other benchmark
profile. A benchmark must decide and version its own dispositions.

The `id` is a stable, human-readable identifier of 1–128 ASCII letters,
digits, `.`, `_`, `-`, `/`, or `:`. The `features` object must contain every
v1 key exactly once. Unknown or duplicate top-level fields, feature keys, or
JSON keys are rejected, as are missing keys and non-string dispositions.

The dispositions form a positive contract:

- `scored` means the dimension remains in both canonical scores and can affect
  size, matching, the chosen edit path, distance, and visualization.
- `ignored` means VeroSim neutralizes the dimension in both canonical scores
  before every comparison-sensitive operation and audits what was removed.
- `unsupported` is reserved for a named dimension that schema v1 does not
  extract canonically. It is a declaration, not a request to score or erase
  source markup.

Schema v1 is immutable. New canonical dimensions or support for a currently
unsupported dimension require a new schema version, so an existing profile
cannot silently inherit a broader scoring surface.

## Feature registry

| Feature keys | Allowed values | Projection behavior |
|---|---|---|
| `note.pitch`, `note.notehead`, `note.dot`, `note.tuplet`, `note.tuplet_info`, `note.grace` | `scored` only | Identity, rhythm, or matching carriers; never neutralized in v1. |
| `note.accidental` | `scored`, `ignored` | Clears the visible accidental value; preserves resolved sounding pitch. |
| `note.tie` | `scored`, `ignored` | Clears the canonical tie bit after extraction and accidental-carry resolution. |
| `note.beam`, `note.articulation`, `note.expression` | `scored`, `ignored` | Clears the corresponding canonical vector. |
| `note.grace_slash` | `scored`, `ignored` | Clears only the slash bit; the grace-note identity remains scored. |
| `note.gap` | `scored`, `ignored` | Sets the canonical gap duration to zero. |
| `extra.arpeggio`, `extra.barline`, `extra.clef`, `extra.hairpin.crescendo`, `extra.hairpin.diminuendo`, `extra.dynamic`, `extra.keysig`, `extra.ottava`, `extra.repeat`, `extra.slur`, `extra.timesig`, `layout.system_break` | `scored`, `ignored` | Erases extras of that canonical kind. Measures made empty are pruned using the normal extraction invariant. |
| `note.ornament`, `note.style`, `note.lyric`, `note.fingering`, `extra.text`, `extra.pedal`, `layout.page_break` | `unsupported` only | Named future/absent dimensions; no v1 canonical value exists to score or project. |

Removed counts use the same symbol units as `notation_size()`. Therefore, for
each side, `projected notation size = fully extracted notation size - removed
total`. Some extras contribute more than one unit; for example, a time
signature carries separate numerator and denominator units.

## Derived semantics and caveats

Projection occurs after parsing and semantic extraction. This ordering matters:

- Ignoring ties clears only the comparison-visible tie bit. Tie-chain and
  accidental-carry resolution have already populated `sounding_alter`, which
  remains intact so effective pitch does not change.
- Ignoring visible accidentals does not undo key-signature, carried accidental,
  or tie-derived sounding pitch. `note.accidental` describes visible accidental
  notation, not pitch semantics.
- Pitch, notehead/duration, dots, tuplets, and grace identity are scored-only in
  v1 because matching and timing depend on their derived fields. They cannot be
  safely removed as independent decorations.
- Unsupported categories may still exist in source files. Schema v1 makes no
  scoring claim about markup that is not represented in the canonical model.
- A profile is a scope contract, not a weighting system. VeroSim's normal edit
  costs still apply to every retained feature.

## Audit output

Profile-aware comparison records add an `evaluation_profile` object. Feature
keys are always serialized in registry order:

```json
{
  "evaluation_profile": {
    "schema": "verosim.evaluation-profile",
    "schema_version": 1,
    "id": "my-benchmark/notation-scope-v1",
    "digest": "sha256:0123456789abcdef...",
    "removed": {
      "pred": {
        "total": 3,
        "features": {
          "note.pitch": 0,
          "note.accidental": 1,
          "note.tie": 2
        }
      },
      "gt": {
        "total": 0,
        "features": {
          "note.pitch": 0,
          "note.accidental": 0,
          "note.tie": 0
        }
      }
    }
  }
}
```

The real `features` maps contain all v1 keys; the excerpt above is shortened.
The digest is lowercase SHA-256 over the exact profile bytes, including JSON
whitespace and the final newline. It identifies the precise external artifact,
not merely equivalent parsed JSON.

Each successfully loaded score is projected and audited even when the other
side fails to load. An unavailable side is `null`. For malformed JSONL records
where neither score string is available, both sides are `null`.

`--batch-jsonl --summary-json` adds the same profile identity at summary scope.
Every `overall` and `by_group` bucket contains aggregate `removed.pred` and
`removed.gt` objects with an `available` side count and registry-ordered
`counts`. HTML reports display the identity and both removal maps. SVG bundles
include the same audit object in `visualization.json`.

## Transcoda integration handoff

Transcoda should consume this as an external, repository-owned contract:

1. Add a versioned profile under a Transcoda-controlled path such as
   `config/evaluation-profiles/transcoda-omr-v1.json`. Do not copy it into
   VeroSim or generate it dynamically per record.
2. Give it a versioned stable `id`, and explicitly classify all v1 keys. Based
   on benchmark ground-truth policy, verify whether ties, slurs, articulations,
   expressions, ornaments, dynamics, hairpins, text, lyrics, fingering,
   pedaling, ottavas, arpeggios, and layout are absent before choosing the
   allowed disposition. Unsupported v1 keys must remain `unsupported`.
3. Convert Legato ABC predictions to MusicXML as usual. Pass the MusicXML
   prediction and unmodified Humdrum/kern target to VeroSim with one profile
   path, for example:

   ```sh
   build/verosim --batch-jsonl transcoda-validation.jsonl \
     --pred-field prediction --gt-field target \
     --pred-format musicxml --gt-format kern \
     --summary-json transcoda-verosim-summary.json \
     --evaluation-profile config/evaluation-profiles/transcoda-omr-v1.json
   ```

   Per-record `prediction_format` and `target_format` overrides remain valid if
   a batch contains mixed sources.
4. Do not pass `--mode` or `--layout`. Continue passing any required
   `--note-position`, `--typed-space-handling`, field, grouping, dedupe, job,
   and failure-score options normally.
5. Persist the VeroSim version/commit, profile path, profile `id`, and emitted
   digest with benchmark results. CI should reject a digest that differs from
   the reviewed Transcoda profile artifact.
6. Use the per-side and summary removal counts as drift checks. A newly nonzero
   category can reveal a converter or dataset scope change even when that
   category is intentionally ignored for OMR-NED.

This boundary keeps VeroSim representation-neutral and reusable while making
the benchmark's evaluated notation scope explicit, reviewable, and reproducible.
