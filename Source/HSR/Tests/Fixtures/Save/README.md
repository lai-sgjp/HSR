# Phase 16 canonical save fixtures

These fixtures are **synthetic envelope format v1 / payload codec v1 / schema v6** provenance. They are generated from the full-domain DTO in `HSRSaveVersionTests.cpp`, slot `P16_Vector`, user `0`, save id components `(1,2,3,4)`, generation `1`, and UTC milliseconds `123456789` through `HSRSaveVersion::EncodeEnvelopeAtUtc`.

`schema-v6-full.sha256` is the lowercase SHA-256 of the complete deterministic envelope bytes. It is a reviewable cross-process fixture manifest rather than a UE save slot.

These are not UE `USaveGame` files and make no claim of compatibility with historical UE binary serialization. Tests regenerate the canonical bytes and compare their complete-envelope digest, then decode and validate before any Runtime prepare/commit call.
