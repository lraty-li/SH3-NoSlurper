from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, idc
ida_auto.auto_wait()
OUT = Path(__file__).with_name("target_funcs2.txt")
targets=[0x496470,0x4FC0F0,0x4EC800,0x4A5A70,0x4A5C90,0x4DAD20,0x46E850]
ida_hexrays.init_hexrays_plugin()
with open(OUT,"w",encoding="utf-8") as f:
    for ea in targets:
        fn=ida_funcs.get_func(ea)
        f.write("="*100+"\n")
        f.write(f"{ea:08X} {idc.get_func_name(ea)} end={fn.end_ea if fn else 0:08X}\n")
        try:f.write(str(ida_hexrays.decompile(ea))+"\n")
        except Exception as e:f.write(str(e)+"\n")
print("wrote",OUT); idc.qexit(0)
