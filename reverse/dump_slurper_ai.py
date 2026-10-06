from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, idautils, idc
ida_auto.auto_wait()
OUT = Path(__file__).with_name("slurper_ai.txt")
targets=[0x4FE480,0x4FE510,0x4FE5A0,0x4FE630,0x4FE6C0,0x4FE750,0x4F4F70,0x4F5030,0x4F6110]
ida_hexrays.init_hexrays_plugin()
with open(OUT,"w",encoding="utf-8") as f:
    for ea in targets:
        fn=ida_funcs.get_func(ea)
        f.write("="*100+"\n")
        f.write(f"FUNC {ea:08X} {idc.get_func_name(ea)} end={fn.end_ea if fn else 0:08X}\n")
        try:
            f.write(str(ida_hexrays.decompile(ea))+"\n")
        except Exception as e:
            f.write(f"<decompile failed {e}>\n")
        f.write("--- XREFS ---\n")
        for x in idautils.XrefsTo(ea):
            f.write(f"{x.frm:08X}: {idc.generate_disasm_line(x.frm,0) or ''}\n")
        f.write("\n")
print("wrote",OUT)
idc.qexit(0)
