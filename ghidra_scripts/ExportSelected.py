#@category LeCroy
# Export selected functions and symbol XREFs for compact reverse-engineering review.
# Usage from analyzeHeadless:
#   -postScript ExportSelected.py <output-dir> <target> [<target> ...]
#
# Targets may be addresses (for example 0x1619a or 1619a) or symbol names
# (for example KeSetEvent). Address targets export decompiled C plus compact
# incoming/outgoing function references. Symbol targets export references and
# the containing caller function names.
#
# Written for Ghidra's Jython script environment.

from ghidra.app.decompiler import DecompInterface
from java.io import File, PrintWriter

args = getScriptArgs()
if len(args) < 2:
    printerr("Usage: ExportSelected.py <output-dir> <target> [<target> ...]")
    exit()

out_dir = File(args[0])
if not out_dir.exists():
    out_dir.mkdirs()

fm = currentProgram.getFunctionManager()
st = currentProgram.getSymbolTable()
rm = currentProgram.getReferenceManager()
listing = currentProgram.getListing()

decomp = DecompInterface()
decomp.openProgram(currentProgram)

def sanitize(s):
    return ''.join([c if c.isalnum() or c in '._-' else '_' for c in s])

def parse_addr(s):
    t = s.lower()
    try:
        if t.startswith("0x"):
            return toAddr(int(t[2:], 16))
        # Treat plain hex-looking targets as addresses.
        if all(c in "0123456789abcdef" for c in t):
            return toAddr(int(t, 16))
    except:
        pass
    return None

def function_name_at_or_containing(addr):
    f = fm.getFunctionAt(addr)
    if f is None:
        f = fm.getFunctionContaining(addr)
    return f

def write_function(f):
    entry = f.getEntryPoint()
    stem = "%s_%s" % (entry, sanitize(f.getName()))

    # Decompiled C
    cfile = File(out_dir, stem + ".c")
    pw = PrintWriter(cfile)
    try:
        res = decomp.decompileFunction(f, 60, monitor)
        if res is not None and res.decompileCompleted():
            pw.print(res.getDecompiledFunction().getC())
        else:
            pw.println("/* decompilation failed */")
            if res is not None:
                pw.println("/* %s */" % res.getErrorMessage())
    finally:
        pw.close()

    # Compact metadata / XREFs
    mfile = File(out_dir, stem + ".refs.txt")
    pw = PrintWriter(mfile)
    try:
        pw.println("FUNCTION %s %s" % (entry, f.getName()))

        pw.println("INCOMING")
        seen = set()
        it = rm.getReferencesTo(entry)
        while it.hasNext():
            r = it.next()
            cf = function_name_at_or_containing(r.getFromAddress())
            key = "%s %s" % (r.getFromAddress(), cf.getName() if cf else "<no-function>")
            if key not in seen:
                seen.add(key)
                pw.println(key)

        pw.println("OUTGOING_FUNCTIONS")
        seen = set()
        ins = listing.getInstructions(f.getBody(), True)
        while ins.hasNext():
            i = ins.next()
            for r in i.getReferencesFrom():
                tf = fm.getFunctionAt(r.getToAddress())
                if tf is not None:
                    key = "%s %s" % (tf.getEntryPoint(), tf.getName())
                    if key not in seen:
                        seen.add(key)
                        pw.println(key)
    finally:
        pw.close()

def write_symbol(name):
    syms = st.getSymbols(name)
    idx = 0
    found = False
    while syms.hasNext():
        found = True
        sym = syms.next()
        addr = sym.getAddress()
        sfile = File(out_dir, "symbol_%s_%d.refs.txt" % (sanitize(name), idx))
        idx += 1
        pw = PrintWriter(sfile)
        try:
            pw.println("SYMBOL %s %s" % (name, addr))
            pw.println("REFERENCES")
            seen = set()
            it = rm.getReferencesTo(addr)
            while it.hasNext():
                r = it.next()
                cf = function_name_at_or_containing(r.getFromAddress())
                key = "%s %s" % (r.getFromAddress(), cf.getName() if cf else "<no-function>")
                if key not in seen:
                    seen.add(key)
                    pw.println(key)
        finally:
            pw.close()
    if not found:
        printerr("Symbol not found: " + name)

for target in args[1:]:
    addr = parse_addr(target)
    if addr is not None:
        f = function_name_at_or_containing(addr)
        if f is None:
            printerr("No function at/containing: " + target)
        else:
            write_function(f)
    else:
        write_symbol(target)

decomp.dispose()
print("ExportSelected.py: wrote results to " + out_dir.getAbsolutePath())
