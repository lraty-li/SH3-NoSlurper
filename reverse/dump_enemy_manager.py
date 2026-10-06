from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, idc, idautils
ida_auto.auto_wait()
OUT = Path(__file__).with_name("enemy_manager.txt")
start=0x4A2E20
fn=ida_funcs.get_func(start)
with open(OUT,"w",encoding="utf-8") as f:
    f.write(f"FUNC {start:08X}-{fn.end_ea:08X}\n")
    ea=start
    while ea < min(fn.end_ea,start+0x100):
        f.write(f"{ea:08X}: {idc.generate_disasm_line(ea,0) or ''}\n")
        ea=idc.next_head(ea,fn.end_ea)
    f.write("\nPSEUDOCODE\n")
    ida_hexrays.init_hexrays_plugin()
    f.write(str(ida_hexrays.decompile(start)))
print("wrote",OUT)
idc.qexit(0)
