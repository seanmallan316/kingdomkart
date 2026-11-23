"""Make the artifact copy of the game: same page without the outer html/head/body tags."""
import re, sys
src = open(sys.argv[1]).read()
for pat in [r'<!doctype html>\s*', r'<html[^>]*>\s*', r'</html>\s*', r'<head>\s*', r'</head>\s*', r'<body>\s*', r'</body>\s*',
            r'<meta charset="utf-8">\s*', r'<meta name="viewport"[^>]*>\s*']:
    src = re.sub(pat, '', src, flags=re.I)
src = src.replace(':root { --ink', ':root { color-scheme: dark; --ink')
open(sys.argv[2], 'w').write(src)
