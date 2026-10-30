# Plans

Work larger than one session gets a plan here.

- A plan says what will be built, in what order, and what proves each phase done. It is written before the work and left as written. What was built goes in `ARCHITECTURE.md`.
- Pick a kebab-case slug for the feature. The slug joins the pieces: `<slug>.md`, `memory/<slug>.md` and `reports/<slug>/`.
- Start a plan from `TEMPLATE.md`, a feature memory from `memory/TEMPLATE.md`, and a report from `reports/TEMPLATE.md`.
- The feature memory holds the facts every session on the feature needs. The session orchestrating the feature is its only writer.
- A report is one file per run, named `<yyyy-mm-dd>-<what-it-did>.md`, and never updated afterwards. File problems a run found and left under `../issues/open/` and link them from the report.
- A plan whose phases have all landed moves to `archive/`, with its memory in `archive/memory/`. Reports stay where they are.
