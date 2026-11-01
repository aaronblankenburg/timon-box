"""Languages of the build guide.

German is the source. The English text sits right next to it in the code:

    T('Deckel', 'Lid')

returns the text for the language being built. The language comes from ASSEMBLY_LANG (de or en); build.sh builds
both. Text that reads the same in both languages (part numbers, signal names, file names) needs no T().

Two checks keep the English version complete:
- T() with only the German text records it in MISSING, the build lists it, and the English PDF shows German there.
- check_untranslated() lists German-looking string literals that are not inside T() at all.
"""
import io, os, re, tokenize

LANG = os.environ.get('ASSEMBLY_LANG', 'de')
MISSING = []


def T(de, en=None):
    """Text in the language being built. de is the German original, en the English translation."""
    if LANG == 'de':
        return de
    if en is None:
        MISSING.append(de)
        return de
    return en


GERMAN = re.compile(r'[äöüÄÖÜß]|\b(und|oder|der|die|das|dem|den|des|ein|eine|einen|mit|auf|für|ist|sind|wird|nicht|'
                    r'zum|zur|vom|beim|bis|nach|unter|über|Seite|Schritt|Kapitel)\b')


def check_untranslated(paths, ignore=()):
    """German-looking string literals outside T(...), as (file, line, text). Docstrings and `ignore` are skipped."""
    found = []
    for path in paths:
        src = open(path, encoding='utf-8').read()
        stack = []                                        # one entry per open bracket: True if it is T(
        prev = None
        for tok in tokenize.generate_tokens(io.StringIO(src).readline):
            if tok.type == tokenize.OP and tok.string in '([{':
                stack.append(tok.string == '(' and prev is not None and prev.type == tokenize.NAME and prev.string == 'T')
            elif tok.type == tokenize.OP and tok.string in ')]}':
                if stack: stack.pop()
            elif tok.type == tokenize.STRING and not tok.string.startswith(('"""', "'''")) and not any(stack):
                try:
                    value = eval(tok.string)                  # a plain literal, see tokenize
                except Exception:
                    value = tok.string
                if isinstance(value, str) and value not in ignore and GERMAN.search(value):
                    found.append((os.path.basename(path), tok.start[0], value))
            if tok.type not in (tokenize.NL, tokenize.NEWLINE, tokenize.COMMENT):
                prev = tok
    return found
