# Pinned versions

| Component             | Version / commit                                                                        | Notes                                                                                                              |
| --------------------- | --------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| verovio (submodule)   | `b9c58d41c66695e36b17e7c7de1e362e30441bf8` (2026-06-11, version-2.7.1-10416-gb9c58d41c) | Parsing front-end (Layer 1), consumed via `BUILD_AS_LIBRARY`                                                       |
| musicdiff (submodule) | `f413e5e9b749bba8100b79425912b7db4a0f7a72` (2026-03-03, v5.2.0-1-gf413e5e)              | Executable spec + validation oracle (D12)                                                                          |
| Catch2                | v3.8.1 (CMake FetchContent)                                                             | Test framework                                                                                                     |
| Python (harness venv) | 3.12 (uv-managed, `harness/.venv`)                                                      | System 3.14 untested with music21; pinned for safety                                                               |
| music21               | **9.9.2 (must stay <10)**                                                               | converter21 4.0.1 calls `Metadata._convertValue`, removed in music21 10 — kern parsing breaks (oracle smoke finding) |
| converter21           | 4.0.1 (latest)                                                                          | music21's Humdrum/kern parser used by musicdiff                                                                    |
| numpy                 | 2.4.6                                                                                   | as resolved                                                                                                        |
