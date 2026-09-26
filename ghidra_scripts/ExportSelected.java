// Export selected functions and symbol XREFs for compact reverse-engineering review.
// @category LeCroy
//
// Usage from analyzeHeadless:
//   -postScript ExportSelected.java <output-dir> <target> [<target> ...]
//
// Targets may be addresses (for example 0x1619a or 1619a) or symbol names
// (for example KeSetEvent). Address targets export decompiled C plus compact
// incoming/outgoing function references. Symbol targets export references and
// the containing caller function names.

import java.io.File;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;

public class ExportSelected extends GhidraScript {
    private File outDir;
    private FunctionManager fm;
    private SymbolTable st;
    private ReferenceManager rm;
    private DecompInterface decomp;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("Usage: ExportSelected.java <output-dir> <target> [<target> ...]");
            return;
        }

        outDir = new File(args[0]);
        if (!outDir.exists() && !outDir.mkdirs()) {
            printerr("Failed to create output directory: " + outDir.getAbsolutePath());
            return;
        }

        fm = currentProgram.getFunctionManager();
        st = currentProgram.getSymbolTable();
        rm = currentProgram.getReferenceManager();

        decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        try {
            for (int i = 1; i < args.length; i++) {
                String target = args[i];
                Address addr = parseAddress(target);

                if (addr != null) {
                    Function f = fm.getFunctionAt(addr);
                    if (f == null) {
                        f = fm.getFunctionContaining(addr);
                    }

                    if (f == null) {
                        printerr("No function at/containing: " + target);
                    }
                    else {
                        writeFunction(f);
                    }
                }
                else {
                    writeSymbol(target);
                }
            }
        }
        finally {
            decomp.dispose();
        }

        println("ExportSelected.java: wrote results to " + outDir.getAbsolutePath());
    }

    private String sanitize(String s) {
        return s.replaceAll("[^A-Za-z0-9._-]", "_");
    }

    private Address parseAddress(String s) {
        String t = s.toLowerCase();
        try {
            long value;
            if (t.startsWith("0x")) {
                value = Long.parseUnsignedLong(t.substring(2), 16);
            }
            else if (t.matches("[0-9a-f]+")) {
                value = Long.parseUnsignedLong(t, 16);
            }
            else {
                return null;
            }
            return toAddr(value);
        }
        catch (Exception e) {
            return null;
        }
    }

    private Function functionAtOrContaining(Address addr) {
        Function f = fm.getFunctionAt(addr);
        if (f == null) {
            f = fm.getFunctionContaining(addr);
        }
        return f;
    }

    private void writeFunction(Function f) throws Exception {
        Address entry = f.getEntryPoint();
        String stem = entry + "_" + sanitize(f.getName());

        File cfile = new File(outDir, stem + ".c");
        try (PrintWriter pw = new PrintWriter(cfile, "UTF-8")) {
            DecompileResults res = decomp.decompileFunction(f, 60, monitor);
            if (res != null && res.decompileCompleted()) {
                pw.print(res.getDecompiledFunction().getC());
            }
            else {
                pw.println("/* decompilation failed */");
                if (res != null) {
                    pw.println("/* " + res.getErrorMessage() + " */");
                }
            }
        }

        File rfile = new File(outDir, stem + ".refs.txt");
        try (PrintWriter pw = new PrintWriter(rfile, "UTF-8")) {
            pw.println("FUNCTION " + entry + " " + f.getName());

            pw.println("INCOMING");
            Set<String> seenIncoming = new HashSet<>();
            ReferenceIterator refsTo = rm.getReferencesTo(entry);
            while (refsTo.hasNext()) {
                Reference r = refsTo.next();
                Function cf = functionAtOrContaining(r.getFromAddress());
                String line = r.getFromAddress() + " " +
                    (cf != null ? cf.getName() : "<no-function>");
                if (seenIncoming.add(line)) {
                    pw.println(line);
                }
            }

            pw.println("OUTGOING_FUNCTIONS");
            Set<String> seenOutgoing = new HashSet<>();
            InstructionIterator ins = currentProgram.getListing().getInstructions(f.getBody(), true);
            while (ins.hasNext()) {
                Instruction inst = ins.next();
                Reference[] refs = inst.getReferencesFrom();
                for (Reference r : refs) {
                    Function tf = fm.getFunctionAt(r.getToAddress());
                    if (tf != null) {
                        String line = tf.getEntryPoint() + " " + tf.getName();
                        if (seenOutgoing.add(line)) {
                            pw.println(line);
                        }
                    }
                }
            }
        }
    }

    private void writeSymbol(String name) throws Exception {
        SymbolIterator syms = st.getSymbols(name);
        int index = 0;
        boolean found = false;

        while (syms.hasNext()) {
            found = true;
            Symbol sym = syms.next();
            Address addr = sym.getAddress();

            File sfile = new File(outDir,
                "symbol_" + sanitize(name) + "_" + index + ".refs.txt");
            index++;

            try (PrintWriter pw = new PrintWriter(sfile, "UTF-8")) {
                pw.println("SYMBOL " + name + " " + addr);
                pw.println("REFERENCES");

                Set<String> seen = new HashSet<>();
                ReferenceIterator refsTo = rm.getReferencesTo(addr);
                while (refsTo.hasNext()) {
                    Reference r = refsTo.next();
                    Function cf = functionAtOrContaining(r.getFromAddress());
                    String line = r.getFromAddress() + " " +
                        (cf != null ? cf.getName() : "<no-function>");
                    if (seen.add(line)) {
                        pw.println(line);
                    }
                }
            }
        }

        if (!found) {
            printerr("Symbol not found: " + name);
        }
    }
}
