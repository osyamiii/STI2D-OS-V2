#!/usr/bin/env python3
"""Convert a PDF into a comments-only STI2D-OS document script.

The calculator reader does not execute imported Python: it reads only the
STI2D-DOC-V1 comment records. PDF pages are extracted on the computer and
encoded into the same safe format used by the portal for TXT/Markdown/CSV.
"""
from __future__ import annotations

import argparse
import re
import sys
import unicodedata
from pathlib import Path

MAX_BYTES = 16 * 1024


def safe_slug(value: str) -> str:
    value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
    value = re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_").lower()
    return (value[:24] or "document")


def safe_title(value: str) -> str:
    value = re.sub(r"[\r\n\x00-\x1f]+", " ", value).strip()
    return (value[:30] or "Document")


def encode_document(title: str, page_texts: list[str], max_bytes: int = MAX_BYTES) -> bytes:
    """Encode extracted PDF text as the reader's comments-only document format."""
    lines = ["# coding: utf-8", "# STI2D-DOC-V1", "# TITLE: " + safe_title(title)]
    for page_number, text in enumerate(page_texts, 1):
        lines.append(f"#|--- Page {page_number} ---")
        normalized = text.replace("\x00", "").replace("\r\n", "\n").replace("\r", "\n")
        for line in normalized.split("\n"):
            # Prefix every line; imported content can never become Python code.
            lines.append("#|" + line)
    payload = ("\n".join(lines) + "\n").encode("utf-8")
    if len(payload) > max_bytes:
        raise ValueError(
            f"Le document converti fait {len(payload)} octets ; "
            f"la limite du lecteur est {max_bytes} octets. "
            "Réduis le nombre de pages ou compresse le contenu avant transfert."
        )
    return payload


def extract_pdf(path: Path, pages: str | None = None) -> tuple[str, list[str]]:
    try:
        from pypdf import PdfReader
    except ImportError as exc:
        raise RuntimeError("Dépendance manquante : installe pypdf avec `python -m pip install pypdf`.") from exc

    try:
        reader = PdfReader(str(path), strict=True)
    except Exception as exc:
        raise ValueError(f"PDF illisible ou endommagé : {exc}") from exc
    if reader.is_encrypted:
        raise ValueError("PDF protégé par mot de passe : déverrouille-le avant conversion.")
    if not reader.pages:
        raise ValueError("Le PDF ne contient aucune page.")

    indexes = list(range(len(reader.pages)))
    if pages:
        selected: list[int] = []
        try:
            for part in pages.split(","):
                part = part.strip()
                if not part:
                    continue
                if "-" in part:
                    start_s, end_s = part.split("-", 1)
                    start, end = int(start_s), int(end_s)
                    if start < 1 or end < start or end > len(reader.pages):
                        raise ValueError
                    selected.extend(range(start - 1, end))
                else:
                    number = int(part)
                    if number < 1 or number > len(reader.pages):
                        raise ValueError
                    selected.append(number - 1)
        except ValueError as exc:
            raise ValueError(f"Sélection de pages invalide (PDF : {len(reader.pages)} pages). Exemple : 1-3,5,8-9.") from exc
        indexes = list(dict.fromkeys(selected))
        if not indexes:
            raise ValueError("Aucune page sélectionnée.")

    title = str(reader.metadata.title or path.stem) if reader.metadata else path.stem
    extracted: list[str] = []
    for index in indexes:
        try:
            extracted.append(reader.pages[index].extract_text() or "[Aucun texte extractible sur cette page]")
        except Exception as exc:
            extracted.append(f"[Extraction impossible pour la page {index + 1} : {exc}]")
    return title, extracted


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdf", type=Path, help="PDF source")
    parser.add_argument("-o", "--output", type=Path, help="fichier .py de sortie")
    parser.add_argument("--pages", help="pages à extraire, ex. 1-3,5,8-9 (par défaut : toutes)")
    parser.add_argument("--max-bytes", type=int, default=MAX_BYTES, help="limite du lecteur (défaut : 16384 octets)")
    args = parser.parse_args(argv)

    if not args.pdf.is_file():
        parser.error(f"fichier introuvable : {args.pdf}")
    if args.max_bytes < 256 or args.max_bytes > MAX_BYTES:
        parser.error(f"--max-bytes doit être compris entre 256 et {MAX_BYTES}")

    try:
        title, pages = extract_pdf(args.pdf, args.pages)
        payload = encode_document(title, pages, args.max_bytes)
        output = args.output or Path("stidoc_" + safe_slug(args.pdf.stem) + ".py")
        if output.suffix.lower() != ".py":
            raise ValueError("Le fichier de sortie doit avoir l'extension .py.")
        output.write_bytes(payload)
    except (ValueError, RuntimeError, OSError) as exc:
        print(f"Erreur : {exc}", file=sys.stderr)
        return 2

    print(f"Document créé : {output} ({len(payload)} octets, {len(pages)} page(s) extraite(s)).")
    print("Transfère ce script Python sur la NumWorks, puis ouvre STI2D-OS > Fichiers.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
