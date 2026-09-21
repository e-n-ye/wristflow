# Project Instructions

- Read `docs/STATUS.md`, `docs/BASELINE.md`, and `docs/BUILD.md` before continuing. Read the reference consensus in full when changing scope or foundational decisions.
- Preserve prior decisions. Do not restart a requirements interview; ask only about new decisions with evidence.
- Use UTF-8 for documents, comments, and Chinese strings. Check only changed Chinese files for encoding corruption.
- Keep SiFli-SDK at the commit and submodule revisions in `sdk.lock.json`. Use its official installation and SCons conventions.
- Build scripts must never flash a device. The user reported ordering hardware on 2026-09-21; it has not arrived and the actual board/screen revisions remain unverified. Do not purchase, contact sellers, or change unrelated projects.
- Separate source inspection, successful compilation, and hardware verification in every result. No hardware means no claims about touch, BLE operation, sleep current, or battery life.
- Record commands, versions, artifacts, failures, and the next bounded experiment in project documents.
- Start with a small working result. Reference the old F411 workflow and sensor code; do not migrate its architecture or CI wholesale.
- Do not batch-delete files or directories. Preserve incomplete downloads and existing user work.
- Subagents are read-only researchers/reviewers. The primary agent makes edits and final decisions.
