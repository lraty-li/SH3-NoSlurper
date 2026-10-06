from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, idautils, idc
ida_auto.auto_wait()
OUT = Path(__file__).with_name("hp_accesses.txt")
ida_hexrays.init_hexrays_plugin()
hits={}
for seg in idautils.Segments():
    ea=seg; end=idc.get_segm_end(seg)
    while ea<end:
        line=idc.generate_disasm_line(ea,0) or ""
        u=line.upper()
        if "+180H]" in u or "+184H]" in u:
            fn=ida_funcs.get_func(ea)
            if fn: hits.setdefault(fn.start_ea,[]).append((ea,line))
        ne=idc.next_head(ea,end)
        if ne==idc.BADADDR or ne<=ea: break
        ea=ne
with open(OUT,"w",encoding="utf-8") as f:
    for fs,items in sorted(hits.items()):
        f.write("="*100+"\n")
        f.write(f"FUNC {fs:08X} {idc.get_func_name(fs)}\n")
        for ea,line in items: f.write(f"{ea:08X}: {line}\n")
        try:
            p=str(ida_hexrays.decompile(fs))
            f.write("--- PSEUDOCODE ---\n"+p+"\n")
        except Exception as e: f.write(f"<decompile failed {e}>\n")
print("wrote",OUT)
idc.qexit(0)
