#!/usr/bin/env python3
import html
import re
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MD = ROOT / "docs" / "servo_program_design_summary.md"
DEFAULT_DOCX = ROOT / "docs" / "servo_program_design_summary.docx"


def esc(text):
    return html.escape(text, quote=False)


def clean_inline(text):
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"\1", text)
    text = re.sub(r"\[([^]]+)\]\(([^)]+)\)", r"\1 (\2)", text)
    return text


def para(text="", style=None):
    style_xml = f'<w:pStyle w:val="{style}"/>' if style else ""
    indent = '<w:ind w:left="360" w:hanging="180"/>' if style == "ListBullet" else ""
    ppr = f"<w:pStyle w:val=\"{style}\"/>{indent}" if style else indent
    return (
        "<w:p><w:pPr>"
        + ppr
        + "</w:pPr><w:r><w:t xml:space=\"preserve\">"
        + esc(text)
        + "</w:t></w:r></w:p>"
    )


def cell(text, width, header=False):
    if header:
        shade = '<w:shd w:fill="D9EAF7"/>'
    elif text.lstrip().startswith("🟨"):
        shade = '<w:shd w:fill="FFF2CC"/>'
    elif text.lstrip().startswith("✅"):
        shade = '<w:shd w:fill="E2F0D9"/>'
    else:
        shade = ""
    bold = "<w:b/>" if header else ""
    font = "<w:sz w:val=\"17\"/><w:szCs w:val=\"17\"/>"
    run = f"<w:r><w:rPr>{bold}{font}</w:rPr><w:t xml:space=\"preserve\">{esc(text)}</w:t></w:r>"
    p = f"<w:p><w:pPr><w:spacing w:after=\"60\"/></w:pPr>{run}</w:p>"
    return (
        f'<w:tc><w:tcPr><w:tcW w:w="{width}" w:type="dxa"/>{shade}'
        f'<w:vAlign w:val="top"/></w:tcPr>{p}</w:tc>'
    )


def table(rows):
    columns = len(rows[0]) if rows else 0
    if columns == 5:
        widths = [650, 3600, 4200, 4600, 1700]
    elif columns == 4:
        widths = [1000, 4400, 5000, 3000]
    else:
        widths = [max(1000, 14200 // max(1, columns))] * columns
    out = [
        "<w:tbl><w:tblPr><w:tblStyle w:val=\"TableGrid\"/>"
        f'<w:tblW w:w="{sum(widths)}" w:type="dxa"/>'
        "<w:tblBorders>"
        "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "<w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"auto\"/>"
        "</w:tblBorders></w:tblPr><w:tblGrid>"
    ]
    out.extend(f'<w:gridCol w:w="{width}"/>' for width in widths)
    out.append("</w:tblGrid>")
    for row_index, row in enumerate(rows):
        header = '<w:trPr><w:tblHeader/></w:trPr>' if row_index == 0 else ""
        out.append("<w:tr>" + header + "".join(
            cell(c, widths[min(i, len(widths) - 1)], row_index == 0)
            for i, c in enumerate(row)
        ) + "</w:tr>")
    out.append("</w:tbl>")
    return "".join(out)


def parse_markdown(md_text):
    blocks = []
    lines = md_text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if not line:
            i += 1
            continue
        if line.startswith("# "):
            blocks.append(("p", clean_inline(line[2:].strip()), "Title"))
            i += 1
            continue
        if line.startswith("#### "):
            blocks.append(("p", clean_inline(line[5:].strip()), "Heading3"))
            i += 1
            continue
        if line.startswith("### "):
            blocks.append(("p", clean_inline(line[4:].strip()), "Heading2"))
            i += 1
            continue
        if line.startswith("## "):
            blocks.append(("p", clean_inline(line[3:].strip()), "Heading1"))
            i += 1
            continue
        if line.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                parts = [clean_inline(p.strip()) for p in lines[i].strip("|").split("|")]
                if not all(re.fullmatch(r":?-{3,}:?", p) for p in parts):
                    rows.append(parts)
                i += 1
            blocks.append(("table", rows, None))
            continue
        if line.startswith("```"):
            code = []
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                code.append(lines[i])
                i += 1
            i += 1
            blocks.append(("p", "\n".join(code), None))
            continue
        if line.startswith("- "):
            blocks.append(("p", "• " + clean_inline(line[2:].strip()), "ListBullet"))
        else:
            blocks.append(("p", clean_inline(line), None))
        i += 1
    return blocks


def content_types():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
<Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>
</Types>"""


def rels():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>"""


def doc_rels():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"/>"""


def styles():
    return """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
<w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:rPr><w:sz w:val="21"/><w:szCs w:val="21"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Title"><w:name w:val="Title"/><w:basedOn w:val="Normal"/><w:rPr><w:b/><w:sz w:val="32"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:rPr><w:b/><w:sz w:val="26"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="heading 2"/><w:basedOn w:val="Normal"/><w:pPr><w:keepNext/></w:pPr><w:rPr><w:b/><w:sz w:val="23"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading3"><w:name w:val="heading 3"/><w:basedOn w:val="Normal"/><w:pPr><w:keepNext/></w:pPr><w:rPr><w:b/><w:sz w:val="21"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="ListBullet"><w:name w:val="List Bullet"/><w:basedOn w:val="Normal"/></w:style>
<w:style w:type="table" w:styleId="TableGrid"><w:name w:val="Table Grid"/><w:tblPr><w:tblBorders><w:top w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:left w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:bottom w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:right w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:insideH w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:insideV w:val="single" w:sz="4" w:space="0" w:color="auto"/></w:tblBorders></w:tblPr></w:style>
</w:styles>"""


def build_docx(md_path, docx_path, landscape=False):
    body = []
    for kind, value, style in parse_markdown(md_path.read_text(encoding="utf-8")):
        if kind == "table":
            body.append(table(value))
        else:
            body.append(para(value, style))

    document = (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
        "<w:body>"
        + "".join(body)
        + ('<w:sectPr><w:pgSz w:w="16838" w:h="11906" w:orient="landscape"/>' if landscape else '<w:sectPr><w:pgSz w:w="11906" w:h="16838"/>')
        + '<w:pgMar w:top="720" w:right="720" w:bottom="720" w:left="720" w:header="360" w:footer="360" w:gutter="0"/></w:sectPr>'
        + "</w:body></w:document>"
    )

    with zipfile.ZipFile(docx_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", content_types())
        z.writestr("_rels/.rels", rels())
        z.writestr("word/_rels/document.xml.rels", doc_rels())
        z.writestr("word/styles.xml", styles())
        z.writestr("word/document.xml", document)


if __name__ == "__main__":
    args = list(sys.argv[1:])
    landscape = "--landscape" in args
    args = [arg for arg in args if arg != "--landscape"]
    md = Path(args[0]).resolve() if len(args) > 0 else DEFAULT_MD
    docx = Path(args[1]).resolve() if len(args) > 1 else DEFAULT_DOCX
    build_docx(md, docx, landscape=landscape)
    print(docx)
