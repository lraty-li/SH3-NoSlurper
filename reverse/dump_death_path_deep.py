from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, idautils, idc
ida_auto.auto_wait()
OUT = Path(__file__).with_name("death_path_deep.txt")
targets=[
0x4F59D0,0x4A38B0,0x474180,0x5D2530,0x5D2550,0x5D2210,0x5D22E0,
0x4F4E30,0x475370,0x474260,0x473B20
]
ida_hexrays.init_hexrays_plugin()
with open(OUT,"w",encoding="utf-8") as f:
    for ea in targets:
        fn=ida_funcs.get_func(ea)
        f.write("="*110+"\n")
        f.write(f"FUNC {ea:08X} {idc.get_func_name(ea)} end={fn.end_ea if fn else 0:08X}\n")
        try:
            f.write(str(ida_hexrays.decompile(ea))+"\n")
        except Exception as e:
            f.write(f"<decompile failed {e}>\n")
        f.write("--- XREFS TO ---\n")
        for x in idautils.XrefsTo(ea):
            f.write(f"{x.frm:08X} {idc.get_func_name(ida_funcs.get_func(x.frm).start_ea) if ida_funcs.get_func(x.frm) else ''}: {idc.generate_disasm_line(x.frm,0) or ''}\n")
        f.write("\n")
print("wrote",OUT)
idc.qexit(0)
