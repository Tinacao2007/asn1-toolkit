#!/usr/bin/env python3
"""Insert tabular function documentation blocks before C++ functions."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCAN_DIRS = (ROOT / "src", ROOT / "include", ROOT / "tools")
MARKER = "Function    :"

CONTROL_START = re.compile(
    r"^(if|for|while|switch|catch|else)\b"
)
SKIP_LINE = re.compile(
    r"^\s*(class|struct|enum|union|namespace|using|typedef|static_assert|"
    r"#|//|/\*|\*|template\s*<|}\s*else)\b"
)
BRACE_ONLY = re.compile(r"^\s*\{\s*$")

# Return-type-ish start for a function definition line.
RET_START = re.compile(
    r"^(\s*)("
    r"void|bool|char|int|long|short|unsigned|signed|float|double|auto|"
    r"std::|asn1::|const\s+|static\s+|inline\s+|virtual\s+|explicit\s+|"
    r"[\w:]+\s*[*&]"
    r")",
    re.IGNORECASE,
)

PARAM_SPLIT = re.compile(r"\([^()]*\)")


def signature_continuation_line(line: str) -> bool:
    stripped = line.strip()
    if not stripped or stripped.startswith("//"):
        return False
    if stripped.endswith("{") or stripped.endswith(";"):
        return False
    return bool(re.search(r"[,\(]", stripped))


def has_tabular_comment(lines: list[str], idx: int) -> bool:
    """True when a tabular block precedes this line (skipping signature continuations)."""
    j = idx - 1
    while j >= 0:
        while j >= 0 and not lines[j].strip():
            j -= 1
        if j < 0:
            return False
        if signature_continuation_line(lines[j]):
            j -= 1
            continue
        if not lines[j].strip().endswith("*/"):
            return False
        k = j
        while k >= 0:
            if MARKER in lines[k] or "| Function |" in lines[k]:
                return True
            if lines[k].strip().startswith("/**"):
                return False
            k -= 1
        return False
    return False


def extract_func_name(signature: str) -> str | None:
    sig = signature.strip()
    sig = re.sub(r"\s+(const|noexcept|override|final)(\s|$)", " ", sig)
    sig = sig.rstrip("{").strip()
    if "(" not in sig:
        return None
    pre = sig[: sig.rfind("(")]
    pre = re.sub(r"\[[^\]]*\]", "", pre)  # drop attributes
    pre = pre.strip()
    if not pre:
        return None
    # Constructor/destructor/operator
    m = re.search(r"(~?[A-Za-z_]\w*|operator\s*[^\s(]+)\s*$", pre)
    if m:
        return m.group(1).strip()
    parts = pre.split()
    if parts:
        return parts[-1].split("::")[-1]
    return None


def parse_params(signature: str) -> list[tuple[str, str]]:
    m = re.search(r"\((.*)\)", signature, re.DOTALL)
    if not m:
        return []
    inner = m.group(1).strip()
    if not inner or inner == "void":
        return []
    params: list[tuple[str, str]] = []
    depth = 0
    chunk = []
    for ch in inner + ",":
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            piece = "".join(chunk).strip()
            chunk = []
            if piece:
                params.append(param_piece(piece))
            continue
        chunk.append(ch)
    return params


def param_piece(piece: str) -> tuple[str, str]:
    piece = piece.strip()
    piece = re.sub(r"\s*=\s*.*$", "", piece)
    # name is last token if it looks like an identifier
    tokens = piece.replace("&", " & ").replace("*", " * ").split()
    name = tokens[-1] if tokens else piece
    if name in ("&", "*", "const"):
        name = tokens[-2] if len(tokens) > 1 else "arg"
    return name.lstrip("*&"), piece


def guess_return_type(signature: str) -> str:
    sig = signature.strip().rstrip("{").strip()
    if "(" not in sig:
        return "unknown"
    pre = sig[: sig.rfind("(")].strip()
    if pre.startswith("~") or pre.endswith("::") or "operator" in pre:
        return "—"
    if not pre:
        return "void"
    # drop function name
    name = extract_func_name(sig + "{")
    if name and pre.endswith(name):
        pre = pre[: -len(name)].strip()
    return pre or "void"


def describe(name: str | None, ret: str, params: list[tuple[str, str]], is_decl: bool) -> str:
    if not name:
        return "Implementation helper."
    n = name
    if n.startswith("~"):
        return f"Destroys the {n[1:]} instance."
    if n.startswith("operator"):
        return f"Implements {n} for this type."
    if n in ("Lexer", "Parser", "Analyzer") or (n and n[0].isupper() and not params):
        return f"Constructs or initializes {n}."
    param_names = ", ".join(p[0] for p in params) if params else "none"
    rlow = ret.lower()
    if "bool" in rlow:
        if n.startswith(("is_", "has_", "can_", "should_", "needs_")):
            tail = n.split("_", 1)[-1].replace("_", " ")
            return f"Returns whether {tail} holds for the given inputs."
        return f"Returns a boolean result from {param_names}."
    if rlow.startswith("void") or ret == "—":
        return f"Performs {n.replace('_', ' ')} ({'declaration' if is_decl else 'definition'})."
    if "std::string" in ret or ret.endswith("string"):
        return f"Builds and returns a string for {n.replace('_', ' ')}."
    if "Result" in ret:
        return f"Returns success or an error from {n.replace('_', ' ')}."
    return f"Computes {n.replace('_', ' ')} from ({param_names})."


def format_block(indent: str, name: str | None, summary: str, params: list[tuple[str, str]], ret: str) -> list[str]:
    fn = name or "(anonymous)"
    if params:
        param_lines = "; ".join(f"{n} — {t}" for n, t in params)
    else:
        param_lines = "none"
    ret_line = ret if ret else "void"
    lines = [
        f"{indent}/**",
        f"{indent} *  Function    : {fn}",
        f"{indent} *  Description : {summary}",
        f"{indent} *  Parameters  : {param_lines}",
        f"{indent} *  Returns     : {ret_line}",
        f"{indent} */",
    ]
    return lines


def collect_signature_lines(lines: list[str], end_idx: int) -> tuple[int, str]:
    """Walk upward from line before `{` to collect a multi-line signature."""
    parts: list[str] = []
    i = end_idx
    open_parens = 0
    while i >= 0:
        line = lines[i].rstrip()
        stripped = line.strip()
        if not stripped:
            if parts:
                break
            i -= 1
            continue
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            break
        if has_tabular_comment(lines, i):
            break
        chunk = stripped.rstrip("{").strip()
        parts.insert(0, chunk)
        open_parens += chunk.count("(") - chunk.count(")")
        if open_parens <= 0 and "(" in " ".join(parts):
            if RET_START.match(line) or re.match(r"^[A-Za-z_~]", stripped):
                break
        if RET_START.match(line) or stripped.startswith("explicit ") or stripped.startswith("virtual "):
            if open_parens <= 0:
                break
        i -= 1
        if end_idx - i > 12:
            break
    return i, " ".join(parts)


def looks_like_lambda(signature: str) -> bool:
    if "=" not in signature:
        return False
    eq = signature.index("=")
    rest = signature[eq + 1 :].lstrip()
    return rest.startswith("[") or rest.startswith("&[") or "&[" in signature


def is_function_definition_line(line: str, lines: list[str] | None = None, idx: int | None = None) -> bool:
    stripped = line.strip()
    one_liner = bool(re.search(r"\)\s*(const|noexcept|override|final|\s)*\{", stripped) and "}" in stripped)
    if not stripped.endswith("{") and not one_liner:
        return False
    if CONTROL_START.match(stripped):
        return False
    if stripped.startswith("}"):
        return False
    if SKIP_LINE.match(line):
        return False
    if "[" in stripped and "]" in stripped:
        return False
    sig = stripped.rstrip("{").strip()
    sig_idx = idx if idx is not None else 0
    if lines is not None and idx is not None:
        sig_idx, sig = collect_signature_lines(lines, idx)
    if "(" not in sig:
        return False
    if lines is not None and idx is not None and idx != sig_idx and "(" not in line:
        return False
    if looks_like_lambda(sig):
        return False
    if not extract_func_name(sig + " {"):
        return False
    if RET_START.match(lines[sig_idx] if lines is not None else line):
        return True
    first = (lines[sig_idx] if lines is not None else line).strip()
    if re.match(r"^~?[A-Za-z_]", first) and re.search(r"\)\s*\{?\s*$", sig):
        return True
    return False


def is_function_declaration(line: str) -> bool:
    stripped = line.strip()
    if not stripped.endswith(";"):
        return False
    if "{" in stripped:
        return False
    if CONTROL_START.match(stripped):
        return False
    if SKIP_LINE.match(line):
        return False
    if "=" in stripped:
        if stripped.endswith("= delete;") or stripped.endswith("= default;"):
            pass
        elif "(" in stripped and stripped.index("(") < stripped.rindex("="):
            pass  # default argument in parameter list
        elif re.search(r"=\s*[^=].*\)\s*;?\s*$", stripped):
            pass  # default on closing line of a multi-line parameter list
        else:
            return False
    if "(" not in stripped:
        return False
    before_semi = stripped[:-1].strip()
    before_semi = re.sub(r"\b(override|final|noexcept|const)\s*$", "", before_semi).strip()
    if not before_semi.endswith(")") and not before_semi.endswith("= 0") and not before_semi.endswith("= default"):
        if not (")" in before_semi and before_semi.rstrip().endswith("0") and "= 0" in stripped):
            return False
    return bool(RET_START.match(line) or re.match(r"^\s*~?\w", line) or "operator" in stripped)


def process_file(path: Path, dry_run: bool) -> int:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)
    raw = [ln.rstrip("\n\r") for ln in lines]
    insertions: list[tuple[int, list[str], int]] = []

    i = 0
    while i < len(raw):
        line = raw[i]
        if BRACE_ONLY.match(line) and i > 0:
            # Signature ends on previous line; `{` on its own line.
            sig_idx, sig = collect_signature_lines(raw, i - 1)
            synthetic = raw[i - 1].rstrip() + " {"
            if "(" in sig and is_function_definition_line(synthetic, raw, i - 1):
                if not has_tabular_comment(raw, i):
                    name = extract_func_name(sig + " {")
                    params = parse_params(sig)
                    ret = guess_return_type(sig)
                    summary = describe(name, ret, params, False)
                    indent = re.match(r"^(\s*)", raw[sig_idx]).group(1)
                    block = format_block(indent, name, summary, params, ret)
                    insertions.append((sig_idx, block, i))
            i += 1
            continue

        if is_function_definition_line(line, raw, i):
            if not has_tabular_comment(raw, i):
                sig_idx, sig = collect_signature_lines(raw, i)
                sig = sig or line.strip()
                name = extract_func_name(sig if sig.endswith("{") else sig + " {")
                params = parse_params(sig)
                ret = guess_return_type(sig)
                summary = describe(name, ret, params, False)
                indent = re.match(r"^(\s*)", raw[sig_idx]).group(1)
                block = format_block(indent, name, summary, params, ret)
                insertions.append((sig_idx, block, i))
            i += 1
            continue

        if path.suffix in (".hpp", ".h") and is_function_declaration(line):
            if not has_tabular_comment(raw, i):
                sig_idx, sig = collect_signature_lines(raw, i)
                sig = sig.rstrip(";").strip()
                if sig.endswith("= 0"):
                    sig = sig[:-3].strip()
                if sig.endswith("= default"):
                    sig = sig[:-9].strip()
                name = extract_func_name(sig + " {")
                params = parse_params(sig)
                ret = guess_return_type(sig)
                summary = describe(name, ret, params, True)
                indent = re.match(r"^(\s*)", raw[sig_idx]).group(1)
                block = format_block(indent, name, summary, params, ret)
                insertions.append((sig_idx, block, i))
            i += 1
            continue

        i += 1

    if not insertions:
        return 0

    seen_span: set[tuple[int, int]] = set()
    unique: list[tuple[int, list[str]]] = []
    for sig_idx, block, end_idx in insertions:
        span = (sig_idx, end_idx)
        if span in seen_span:
            continue
        if has_tabular_comment(raw, end_idx):
            continue
        seen_span.add(span)
        unique.append((sig_idx, block))

    insertions = unique
    insertions.sort(key=lambda x: x[0], reverse=True)
    for idx, block in insertions:
        raw[idx:idx] = block

    if not dry_run:
        path.write_text("\n".join(raw) + "\n", encoding="utf-8", newline="\n")
    return len(insertions)


def main() -> int:
    dry_run = "--dry-run" in sys.argv
    total = 0
    files = 0
    for base in SCAN_DIRS:
        for path in sorted(base.rglob("*")):
            if path.suffix not in (".cpp", ".hpp", ".h"):
                continue
            n = process_file(path, dry_run)
            if n:
                files += 1
                total += n
                print(f"{'would add' if dry_run else 'added'} {n:4d}  {path.relative_to(ROOT)}")
    print(f"{'Would insert' if dry_run else 'Inserted'} {total} blocks in {files} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
