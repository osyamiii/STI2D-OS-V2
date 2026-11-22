import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "pdf_to_stidoc.py"
spec = importlib.util.spec_from_file_location("pdf_to_stidoc", SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class PdfDocumentEncodingTests(unittest.TestCase):
    def test_document_uses_only_comment_records(self):
        payload = mod.encode_document("Cours STI2D", ["ligne 1\nprint('not code')", "deuxième page"])
        text = payload.decode("utf-8")
        self.assertIn("# STI2D-DOC-V1", text)
        self.assertIn("# TITLE: Cours STI2D", text)
        self.assertIn("#|print('not code')", text)
        self.assertIn("#|--- Page 2 ---", text)
        self.assertTrue(all(line.startswith("#") for line in text.splitlines()))

    def test_title_is_sanitized(self):
        payload = mod.encode_document("Titre\n# injection", ["ok"])
        self.assertIn(b"# TITLE: Titre # injection", payload)

    def test_size_limit_is_enforced(self):
        with self.assertRaisesRegex(ValueError, "limite du lecteur"):
            mod.encode_document("Cours", ["x" * 400], max_bytes=256)

    def test_slug_is_portable(self):
        self.assertEqual(mod.safe_slug("Économie & STI2D.pdf"), "economie_sti2d_pdf")


if __name__ == "__main__":
    unittest.main()
