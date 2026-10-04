// Export selected functions and symbol XREFs for compact reverse-engineering review.
// @category LeCroy
//
// Usage from analyzeHeadless:
//   -postScript ExportSelected.java <output-dir> <target> [<target> ...]
//
// Targets may be addresses (for example 0x1619a or 1619a), symbol names
// (for example KeSetEvent), full coverage audit via coverage,
// and explicit recover:11018 to create reviewed missing functions.
// recover: targets MODIFY the local Ghidra project database; back it up first.
// field displacement scans such as field:2e0,
// full function instruction exports such as asm:18194, arbitrary address
// reference scans such as xref:1c8bc, or raw pointer-table snapshots such as
// dwords:1c62c:16 (base address in hex, count in decimal, 1..64).
// Address targets export decompiled C plus compact incoming/outgoing function
// references. Symbol targets export references and the containing caller
// function names. Field scans export every instruction text containing the
// selected displacement.

import java.io.File;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.CodeUnitIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
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
                Address addr = parseTargetAddress(target);

                if (target.toLowerCase().startsWith("recover:")) {
                    Address candidate = parseTargetAddress(
                        target.substring("recover:".length()));
                    if (candidate == null) {
                        printerr("Invalid recover target: " + target);
                    }
                    else {
                        recoverFunction(candidate);
                    }
                }
                else if (target.equalsIgnoreCase("coverage")) {
                    writeCodeCoverage();
                }
                else if (target.equalsIgnoreCase("inventory")) {
                    writeFunctionInventory();
                }
                else if (target.toLowerCase().startsWith("field:")) {
                    writeFieldScan(target.substring("field:".length()));
                }
                else if (target.toLowerCase().startsWith("dwords:")) {
                    String[] spec = target.substring("dwords:".length()).split(":");
                    Address base = spec.length > 0 ? parseTargetAddress(spec[0]) : null;
                    int count = -1;
                    if (spec.length == 2) {
                        try {
                            count = Integer.parseInt(spec[1]);
                        }
                        catch (NumberFormatException ignored) {
                            count = -1;
                        }
                    }
                    if (base == null || count < 1 || count > 64) {
                        printerr("Invalid dwords target (use dwords:<hexbase>:<1..64>): " + target);
                    }
                    else {
                        writeDwordTable(base, count, target);
                    }
                }
                else if (target.toLowerCase().startsWith("asm:")) {
                    Address asmAddr = parseTargetAddress(
                        target.substring("asm:".length()));
                    if (asmAddr == null) {
                        printerr("Invalid asm target: " + target);
                    }
                    else {
                        writeFunctionInstructions(asmAddr, target);
                    }
                }
                else if (target.toLowerCase().startsWith("xref:")) {
                    Address xrefAddr = parseTargetAddress(
                        target.substring("xref:".length()));
                    if (xrefAddr == null) {
                        printerr("Invalid xref target: " + target);
                    }
                    else {
                        writeAddressReferences(xrefAddr, target);
                    }
                }
                else if (addr != null) {
                    Function f = fm.getFunctionAt(addr);
                    if (f == null) {
                        f = fm.getFunctionContaining(addr);
                    }

                    if (f == null) {
                        writeAddressWindow(addr, target);
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

    private Address parseTargetAddress(String s) {
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

    /*
     * Opt-in Ghidra function creation for *reviewed* orphan code entrypoints.
     * This changes the local Ghidra program database: never call this
     * automatically from a census or heuristic. Back up the Ghidra project
     * before using recover: entries. Existing function bodies are protected.
     */
    private void recoverFunction(Address entry) throws Exception {
        String reportName = "RECOVER_" + entry + ".txt";
        File report = new File(outDir, reportName);
        try (PrintWriter pw = new PrintWriter(report, "UTF-8")) {
            pw.println("REVIEWED_ORPHAN_FUNCTION_RECOVERY " + entry);
            MemoryBlock block = currentProgram.getMemory().getBlock(entry);
            if (block == null || !block.isExecute() ||
                    currentProgram.getListing().getInstructionAt(entry) == null) {
                pw.println("REJECTED: not a decoded executable instruction start");
                printerr("recover rejected: " + entry);
                return;
            }
            Function existing = functionAtOrContaining(entry);
            if (existing != null) {
                if (existing.getEntryPoint().equals(entry)) {
                    pw.println("ALREADY_FUNCTION " + existing.getName());
                    writeFunction(existing);
                }
                else {
                    pw.println("REJECTED: inside existing function " +
                        existing.getEntryPoint() + " " + existing.getName());
                    printerr("recover overlaps function: " + entry);
                }
                return;
            }
            CreateFunctionCmd cmd = new CreateFunctionCmd(entry);
            boolean success = cmd.applyTo(currentProgram, monitor);
            Function created = fm.getFunctionAt(entry);
            if (!success || created == null) {
                pw.println("FAILED: Ghidra could not form a function at " + entry);
                printerr("recover failed: " + entry);
                return;
            }
            pw.println("CREATED " + created.getName());
            pw.println("BODY_ADDRESSES " + created.getBody().getNumAddresses());
            pw.println("This recovery updates the local Ghidra project.");
            decomp.flushCache();
            writeFunction(created);
        }
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

    private void writeAddressWindow(Address center, String target) throws Exception {
        String stem = "raw_" + sanitize(target);
        File rfile = new File(outDir, stem + ".asm.txt");

        long window = 0x80;
        Address start = center.subtract(window);
        Address end = center.add(window);

        try (PrintWriter pw = new PrintWriter(rfile, "UTF-8")) {
            pw.println("RAW WINDOW around " + center);
            pw.println("START " + start + " END " + end);

            InstructionIterator ins =
                currentProgram.getListing().getInstructions(start, true);

            while (ins.hasNext()) {
                Instruction inst = ins.next();
                if (inst.getAddress().compareTo(end) > 0) {
                    break;
                }

                Function cf = functionAtOrContaining(inst.getAddress());
                pw.print(inst.getAddress());
                pw.print("  ");
                pw.print(inst.toString());
                if (cf != null) {
                    pw.print("    ; ");
                    pw.print(cf.getName());
                }
                pw.println();

                for (Reference r : inst.getReferencesFrom()) {
                    Function tf = fm.getFunctionAt(r.getToAddress());
                    pw.print("    -> ");
                    pw.print(r.getToAddress());
                    if (tf != null) {
                        pw.print(" ");
                        pw.print(tf.getName());
                    }
                    pw.println();
                }
            }
        }

        printerr("No function at/containing: " + target +
            "; exported raw instruction window instead");
    }

    private void writeFunctionInstructions(Address addr, String target) throws Exception {
        Function f = functionAtOrContaining(addr);
        if (f == null) {
            writeAddressWindow(addr, target);
            return;
        }

        File afile = new File(outDir, "asm_" + sanitize(target) + ".txt");
        try (PrintWriter pw = new PrintWriter(afile, "UTF-8")) {
            pw.println("FUNCTION_ASM " + f.getEntryPoint() + " " + f.getName());
            InstructionIterator ins =
                currentProgram.getListing().getInstructions(f.getBody(), true);
            while (ins.hasNext()) {
                Instruction inst = ins.next();
                pw.println(inst.getAddress() + "  " + inst.toString());
                for (Reference r : inst.getReferencesFrom()) {
                    Function tf = fm.getFunctionAt(r.getToAddress());
                    pw.print("    -> " + r.getToAddress());
                    if (tf != null) {
                        pw.print(" " + tf.getName());
                    }
                    pw.println();
                }
            }
        }
    }

    private void writeAddressReferences(Address addr, String target) throws Exception {
        File rfile = new File(outDir, "xref_" + sanitize(target) + ".txt");
        try (PrintWriter pw = new PrintWriter(rfile, "UTF-8")) {
            pw.println("ADDRESS_XREF " + addr);
            pw.println("REFERENCES");
            Set<String> seen = new HashSet<>();
            ReferenceIterator refsTo = rm.getReferencesTo(addr);
            while (refsTo.hasNext()) {
                Reference r = refsTo.next();
                Function cf = functionAtOrContaining(r.getFromAddress());
                String line = r.getFromAddress() + " " +
                    (cf != null ? cf.getName() : "<no-function>") + " " +
                    r.getReferenceType();
                if (seen.add(line)) {
                    pw.println(line);
                }
            }
        }
    }

    /*
     * Snapshot a literal vtable/other DWORD table independently of Ghidra's
     * symbolic pointer naming. The +0x24 slot gets a short instruction
     * preview so the return convention (RET versus RET 0x8, etc.) can be
     * checked without guessing the target function's name or address.
     */
    private void writeDwordTable(Address base, int count, String target)
            throws Exception {
        File file = new File(outDir, "dwords_" + sanitize(target) + ".txt");
        try (PrintWriter pw = new PrintWriter(file, "UTF-8")) {
            pw.println("DWORD_TABLE " + base + " COUNT " + count);
            for (int i = 0; i < count; ++i) {
                int offset = i * 4;
                try {
                    Address entry = base.add(offset);
                    long raw = getInt(entry) & 0xffffffffL;
                    Address pointer = toAddr(raw);
                    Function pointedFunction = fm.getFunctionAt(pointer);
                    pw.printf("+0x%02X @%s -> 0x%08X", offset, entry, raw);
                    if (pointedFunction != null) {
                        pw.print(" " + pointedFunction.getName());
                    }
                    pw.println();

                    if (offset == 0x24) {
                        pw.println("SLOT_0x24_INSTRUCTION_PREVIEW");
                        Instruction inst =
                            currentProgram.getListing().getInstructionAt(pointer);
                        for (int j = 0; j < 24 && inst != null; ++j) {
                            pw.println("  " + inst.getAddress() + "  " + inst);
                            if (inst.getMnemonicString().toUpperCase().startsWith("RET")) {
                                break;
                            }
                            inst = currentProgram.getListing()
                                .getInstructionAfter(inst.getAddress());
                        }
                        pw.println("END_SLOT_0x24_PREVIEW");
                    }
                }
                catch (Exception ex) {
                    pw.printf("+0x%02X <unreadable: %s>%n",
                        offset, ex.getMessage());
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

                pw.println("POINTER_DWORDS");
                for (int off = 0; off <= 0x40; off += 4) {
                    try {
                        Address paddr = addr.add(off);
                        int raw = getInt(paddr);
                        long unsigned = raw & 0xffffffffL;
                        Address target = toAddr(unsigned);
                        Function tf = fm.getFunctionAt(target);
                        pw.print(String.format("+0x%02X 0x%08X", off, unsigned));
                        if (tf != null) {
                            pw.print(" " + tf.getName());
                        }
                        pw.println();
                    }
                    catch (Exception ignored) {
                        pw.println(String.format("+0x%02X <unreadable>", off));
                    }
                }
            }
        }

        if (!found) {
            printerr("Symbol not found: " + name);
        }
    }


    /*
     * Audit executable instruction coverage separately from the function
     * inventory. Ghidra can disassemble short thunks/indirect-call targets
     * without treating them as functions. Report each contiguous region of
     * decoded executable instructions outside recognized function bodies.
     *
     * This is a decoded-instruction audit, NOT a proof that all executable
     * bytes are code or that all possible indirect targets are discovered.
     */
    private void writeCodeCoverage() throws Exception {
        File file = new File(outDir, "CODE_COVERAGE.txt");
        long executableBlockBytes = 0;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (block.isExecute()) {
                executableBlockBytes += block.getSize();
            }
        }

        long ownedBytes = 0;
        long orphanBytes = 0;
        long ownedInstructions = 0;
        long orphanInstructions = 0;
        long clusterCount = 0;
        Address clusterStart = null;
        Address clusterEnd = null;
        long clusterBytes = 0;
        long clusterInst = 0;

        File refFile = new File(outDir, "UNOWNED_CODE_REFS.txt");
        File asmFile = new File(outDir, "UNOWNED_CODE_ASM.txt");
        try (PrintWriter pw = new PrintWriter(file, "UTF-8");
             PrintWriter refsPw = new PrintWriter(refFile, "UTF-8");
             PrintWriter asmPw = new PrintWriter(asmFile, "UTF-8")) {
            asmPw.println("DECODED_EXECUTABLE_INSTRUCTIONS_OUTSIDE_FUNCTIONS");
            asmPw.println("Refer to CODE_COVERAGE.txt for cluster ranges and");
            asmPw.println("UNOWNED_CODE_REFS.txt for incoming references.");
            refsPw.println("UNOWNED_EXECUTABLE_INSTRUCTION_REFERENCES");
            refsPw.println("target|source|type|source_function");
            pw.println("EXECUTABLE_INSTRUCTION_COVERAGE");
            pw.println("CLUSTERS: contiguous decoded executable instructions not");
            pw.println("contained in any Ghidra-recognized function body.");
            pw.println("start|end|bytes|instructions|start_xrefs");

            InstructionIterator it =
                currentProgram.getListing().getInstructions(true);
            while (it.hasNext()) {
                Instruction inst = it.next();
                Address at = inst.getAddress();
                MemoryBlock block = currentProgram.getMemory().getBlock(at);
                if (block == null || !block.isExecute()) {
                    continue;
                }
                Function owner = fm.getFunctionContaining(at);
                if (owner != null) {
                    ownedBytes += inst.getLength();
                    ownedInstructions++;
                }
                else {
                    orphanBytes += inst.getLength();
                    orphanInstructions++;
                    // Referenced entry points may lie in the middle of a
                    // contiguous unowned instruction region.
                    ReferenceIterator refsTo = rm.getReferencesTo(at);
                    while (refsTo.hasNext()) {
                        Reference ref = refsTo.next();
                        Function src = fm.getFunctionContaining(ref.getFromAddress());
                        refsPw.printf("%s|%s|%s|%s%n", at,
                            ref.getFromAddress(), ref.getReferenceType(),
                            src != null ? src.getName() : "<no-function>");
                    }
                }

                boolean extend = owner == null && clusterStart != null &&
                    clusterEnd.next() != null &&
                    clusterEnd.next().equals(at);
                if (clusterStart != null && !extend) {
                    writeOrphanCodeCluster(pw, clusterStart, clusterEnd,
                        clusterBytes, clusterInst);
                    clusterCount++;
                    clusterStart = null;
                }
                if (owner == null) {
                    if (clusterStart == null) {
                        clusterStart = at;
                        clusterBytes = 0;
                        clusterInst = 0;
                        asmPw.println();
                        asmPw.println("CLUSTER_START " + at);
                    }
                    asmPw.println(at + "  " + inst.toString());
                    clusterEnd = inst.getMaxAddress();
                    clusterBytes += inst.getLength();
                    clusterInst++;
                }
            }
            if (clusterStart != null) {
                writeOrphanCodeCluster(pw, clusterStart, clusterEnd,
                    clusterBytes, clusterInst);
                clusterCount++;
            }
            pw.println("SUMMARY");
            long localFunctions = 0;
            Iterator<Function> known = fm.getFunctions(true).iterator();
            while (known.hasNext()) {
                known.next();
                localFunctions++;
            }
            pw.println("recognized_local_functions=" + localFunctions);
            pw.println("recognized_functions_including_externals=" +
                fm.getFunctionCount());
            pw.println("executable_memory_block_bytes=" + executableBlockBytes);
            pw.println("decoded_owned_bytes=" + ownedBytes);
            pw.println("decoded_unowned_bytes=" + orphanBytes);
            pw.println("decoded_owned_instructions=" + ownedInstructions);
            pw.println("decoded_unowned_instructions=" + orphanInstructions);
            pw.println("unowned_clusters=" + clusterCount);
            pw.println("NOTE: gaps in decoded instruction coverage can include");
            pw.println("data, alignment, undecoded code or embedded tables.");
        }
        writeExecutableByteClassification();
    }

    /*
     * Inspect all code units in executable blocks to distinguish decoded
     * instruction bytes from defined data and undefined bytes. An executable
     * permission on a section is not proof that each byte contains code.
     */
    private void writeExecutableByteClassification() throws Exception {
        File file = new File(outDir, "EXECUTABLE_BYTE_CLASSIFICATION.txt");
        try (PrintWriter pw = new PrintWriter(file, "UTF-8")) {
            pw.println("EXECUTABLE_BYTE_CLASSIFICATION");
            pw.println("block|start|end|bytes|instruction|defined_data|undefined");
            long allInstruction = 0;
            long allData = 0;
            long allUndefined = 0;
            long allBlockBytes = 0;
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                if (!block.isExecute()) {
                    continue;
                }
                long insBytes = 0;
                long dataBytes = 0;
                long undefBytes = 0;
                CodeUnitIterator it = currentProgram.getListing()
                    .getCodeUnits(block.getStart(), true);
                while (it.hasNext()) {
                    CodeUnit cu = it.next();
                    if (cu.getAddress().compareTo(block.getEnd()) > 0) {
                        break;
                    }
                    long len = cu.getLength();
                    if (cu instanceof Instruction) {
                        insBytes += len;
                    }
                    else if (cu instanceof Data && ((Data)cu).isDefined()) {
                        dataBytes += len;
                    }
                    else {
                        undefBytes += len;
                    }
                }
                long accounted = insBytes + dataBytes + undefBytes;
                // Unexpected gaps are explicitly counted as undefined,
                // rather than silently reporting them as decoded code.
                long extra = block.getSize() - accounted;
                if (extra > 0) {
                    undefBytes += extra;
                }
                pw.printf("%s|%s|%s|%d|%d|%d|%d%n",
                    block.getName(), block.getStart(), block.getEnd(),
                    block.getSize(), insBytes, dataBytes, undefBytes);
                allInstruction += insBytes;
                allData += dataBytes;
                allUndefined += undefBytes;
                allBlockBytes += block.getSize();
            }
            pw.printf("TOTAL| | |%d|%d|%d|%d%n",
                allBlockBytes, allInstruction, allData, allUndefined);
            pw.println("NOTE: Defined data or undefined bytes may conceal");
            pw.println("undiscovered code, alignment or embedded constants.");
        }
    }

    private void writeOrphanCodeCluster(PrintWriter pw, Address start,
            Address end, long bytes, long instructions) {
        long refs = 0;
        ReferenceIterator it = rm.getReferencesTo(start);
        while (it.hasNext()) {
            it.next();
            refs++;
        }
        pw.printf("%s|%s|%d|%d|%d%n",
            start, end, bytes, instructions, refs);
    }

    private void writeFunctionInventory() throws Exception {
        File file = new File(outDir, "FUNCTION_INVENTORY.txt");
        try (PrintWriter pw = new PrintWriter(file, "UTF-8")) {
            pw.println("FUNCTION_INVENTORY");
            pw.println("entry|name|body_bytes|selected_c_present|incoming_refs|outgoing_functions");

            Iterator<Function> functions = fm.getFunctions(true).iterator();
            while (functions.hasNext()) {
                Function f = functions.next();
                Address entry = f.getEntryPoint();
                String stem = entry + "_" + sanitize(f.getName());
                File selectedC = new File(outDir, stem + ".c");

                long bodyBytes = f.getBody().getNumAddresses();

                int incomingCount = 0;
                ReferenceIterator refsTo = rm.getReferencesTo(entry);
                while (refsTo.hasNext()) {
                    refsTo.next();
                    incomingCount++;
                }

                Set<String> outgoing = new HashSet<>();
                InstructionIterator ins =
                    currentProgram.getListing().getInstructions(f.getBody(), true);
                while (ins.hasNext()) {
                    Instruction inst = ins.next();
                    for (Reference r : inst.getReferencesFrom()) {
                        Function tf = fm.getFunctionAt(r.getToAddress());
                        if (tf != null) {
                            outgoing.add(tf.getEntryPoint().toString());
                        }
                    }
                }

                pw.print(entry);
                pw.print("|");
                pw.print(f.getName());
                pw.print("|");
                pw.print(bodyBytes);
                pw.print("|");
                pw.print(selectedC.exists() ? "yes" : "no");
                pw.print("|");
                pw.print(incomingCount);
                pw.print("|");
                pw.println(outgoing.size());
            }
        }
    }

    private void writeFieldScan(String displacementText) throws Exception {
        String normalized = displacementText.toLowerCase();
        if (normalized.startsWith("0x")) {
            normalized = normalized.substring(2);
        }
        long value = Long.parseUnsignedLong(normalized, 16);
        String hex = "0x" + Long.toHexString(value);
        String decimal = Long.toString(value);

        File sfile = new File(outDir, "field_" + sanitize(hex) + ".refs.txt");
        try (PrintWriter pw = new PrintWriter(sfile, "UTF-8")) {
            pw.println("FIELD_SCAN " + hex + " (" + decimal + ")");
            pw.println("MATCHES");

            InstructionIterator ins =
                currentProgram.getListing().getInstructions(true);
            Set<String> seen = new HashSet<>();
            while (ins.hasNext()) {
                Instruction inst = ins.next();
                String text = inst.toString().toLowerCase();
                boolean match =
                    text.contains(hex) ||
                    text.contains("+ " + decimal) ||
                    text.contains("+0x" + Long.toHexString(value)) ||
                    text.contains("+ 0x" + Long.toHexString(value));
                if (!match) {
                    continue;
                }

                Function cf = functionAtOrContaining(inst.getAddress());
                String line = inst.getAddress() + " " +
                    (cf != null ? cf.getName() : "<no-function>") +
                    "  " + inst.toString();
                if (seen.add(line)) {
                    pw.println(line);
                    for (Reference r : inst.getReferencesFrom()) {
                        Function tf = fm.getFunctionAt(r.getToAddress());
                        pw.print("    -> ");
                        pw.print(r.getToAddress());
                        if (tf != null) {
                            pw.print(" ");
                            pw.print(tf.getName());
                        }
                        pw.println();
                    }
                }
            }
        }
    }
}
