from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, idc
ida_auto.auto_wait()
OUT = Path(__file__).with_name("dead_state_funcs.txt")
targets=[0x5D2550,0x4F6110,0x4F5030,0x4F4E30,0x475370,0x5D2210,0x5D2230]
ida_hexrays.init_hexrays_plugin()
with open(OUT,"w",encoding="utf-8") as f:
    for ea in targets:
        fn=ida_funcs.get_func(ea); f.write("="*100+"\n"); f.write(f"{ea:08X} {idc.get_func_name(ea)} end={fn.end_ea if fn else 0:08X}\n")
        try:f.write(str(ida_hexrays.decompile(ea))+"\n")
        except Exception as e:f.write(str(e)+"\n")
print("wrote",OUT); idc.qexit(0)
