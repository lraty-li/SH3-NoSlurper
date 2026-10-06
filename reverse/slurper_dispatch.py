from pathlib import Path
import ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_idaapi, idautils, idc
ida_auto.auto_wait()
OUT = Path(__file__).with_name("slurper_dispatch.txt")

def decomp(ea):
    try:
        ida_hexrays.init_hexrays_plugin()
        return str(ida_hexrays.decompile(ea))
    except Exception as e:
        return f"<decompile failed: {e}>"

with open(OUT,"w",encoding="utf-8") as f:
    jt=idc.get_name_ea_simple("jpt_4B3C13")
    f.write(f"jump_table={jt:08X}\n")
    if jt != ida_idaapi.BADADDR:
        for i in range(12):
            tgt=ida_bytes.get_dword(jt+i*4)
            kind=0x20A+i
            f.write(f"kind {kind:04X} -> {tgt:08X}\n")
            # walk a few instructions to the first direct call
            ea=tgt
            for _ in range(16):
                line=idc.generate_disasm_line(ea,0) or ""
                f.write(f"  {ea:08X}: {line}\n")
                ins=idautils.DecodeInstruction(ea)
                if not ins: break
                if idc.print_insn_mnem(ea).lower()=="call":
                    op=idc.get_operand_value(ea,0)
                    if op:
                        fs=ida_funcs.get_func(op)
                        f.write(f"  CALL_TARGET={op:08X} {idc.get_func_name(op)}\n")
                        if fs:
                            f.write("--- DECOMPILE TARGET ---\n")
                            f.write(decomp(fs.start_ea)+"\n")
                    break
                ea=idc.next_head(ea, tgt+0x80)
            f.write("\n")
print("wrote",OUT)
idc.qexit(0)
