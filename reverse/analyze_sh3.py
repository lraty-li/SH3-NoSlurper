from pathlib import Path
import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_idaapi
import ida_kernwin
import ida_ua
import idautils
import idc
import os
import sys

ida_auto.auto_wait()

OUT = Path(__file__).with_name("ida_report.txt")

def insn_has_imm(ea, values):
    insn = ida_ua.insn_t()
    if ida_ua.decode_insn(insn, ea) == 0:
        return False
    for op in insn.ops:
        if op.type == ida_ua.o_void:
            break
        if op.type == ida_ua.o_imm and op.value in values:
            return True
    return False

def decompile_func(start):
    try:
        if not ida_hexrays.init_hexrays_plugin():
            return "<Hex-Rays unavailable>"
        cfunc = ida_hexrays.decompile(start)
        return str(cfunc)
    except Exception as e:
        return f"<decompile failed: {e}>"

values = {0x20A, 0x20B, 0x17, 0x18, 0x16}
candidate_funcs = {}
for seg_ea in idautils.Segments():
    seg_end = idc.get_segm_end(seg_ea)
    ea = seg_ea
    while ea != ida_idaapi.BADADDR and ea < seg_end:
        if ida_bytes.is_code(ida_bytes.get_full_flags(ea)) and insn_has_imm(ea, values):
            f = ida_funcs.get_func(ea)
            if f:
                candidate_funcs.setdefault(f.start_ea, []).append(ea)
        ea = idc.next_head(ea, seg_end)

with open(OUT, "w", encoding="utf-8") as out:
    out.write("SH3 IDA constant/candidate report\n")
    out.write("Input: %s\n\n" % idc.get_input_file_path())
    for fstart in sorted(candidate_funcs):
        fend = ida_funcs.get_func(fstart).end_ea
        name = idc.get_func_name(fstart)
        out.write("=" * 100 + "\n")
        out.write(f"FUNC {fstart:08X}-{fend:08X} {name}\n")
        out.write("Immediate hits: " + ", ".join(f"{ea:08X}" for ea in candidate_funcs[fstart]) + "\n")
        out.write("-" * 100 + "\n")
        for ea in idautils.FuncItems(fstart):
            if any(abs(ea-hit) <= 0x80 for hit in candidate_funcs[fstart]):
                out.write(f"{ea:08X}: {idc.generate_disasm_line(ea, 0) or ''}\n")
        out.write("\n--- PSEUDOCODE ---\n")
        out.write(decompile_func(fstart))
        out.write("\n\n")

print(f"Wrote {OUT}")
idc.qexit(0)
