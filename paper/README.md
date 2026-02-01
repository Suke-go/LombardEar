# LombardEar Paper (ACM Poster/Short Paper draft)

This folder contains an **ACM Primary Article Template** draft in **single-column** format suitable for review.

## Files

- `main.tex`: Manuscript source (single-column review format via `acmart`)
- `references.bib`: BibTeX database (TODO: fill with actual citations)

## Build (local)

Use any LaTeX distribution (TeX Live / MiKTeX).

```bash
latexmk -pdf main.tex
```

## Notes for ACM TAPS

- Review submission: typically **single-column** (`acmart` `manuscript`).
- Publication-ready: often **two-column** (`acmart` `sigconf`) before uploading to TAPS.
- Do **not** upload generated PDFs as source; upload the LaTeX sources (`.tex`, `.bib`, figures).

