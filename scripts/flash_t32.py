import lauterbach.trace32.rcl as t32
from pathlib import Path

dbg = t32.connect()
# Dir where we're calling the script from, e.g. an example project dir
basepath = Path.cwd()
elfpath = basepath / "build"/"main.elf"

dbg.print(f"Flashing via python rcl {elfpath}...")


# FIXME: we have to do it twice -- the first time it fails on attach
# Doing either reset or system.reset makes it always fail

dbg.cmd("reset")
# dbg.cmd("SYSTEM.reset")

try:
    dbg.cmd("system.RESETOUT")
except:
    print("First resetout failed")
    dbg.cmd("wait 0.5")
    dbg.cmd("SYSTEM.reset")
    dbg.cmd("system.RESETOUT")

dbg.cmd("wait 2.0")

dbg.cmd("SYStem.CPU STM32MP257F-CA35")
dbg.cmd("SYStem.config DEBUGPORTTYPE SWD ")
dbg.cmd("SYStem.JtagClock 10MHz")

dbg.cmd("Trace.DISable")
dbg.cmd("SYStem.MemAccess DAP")
dbg.cmd("system.Option DUALPORT on")

try:
    dbg.cmd("SYSTEM.attach")
except:
    print("First attach failed")
    dbg.cmd("wait 0.5")
    dbg.cmd("SYStem.CPU STM32MP257F-CA35")
    dbg.cmd("SYStem.config DEBUGPORTTYPE SWD ")
    dbg.cmd("SYStem.JtagClock 10MHz")
    dbg.cmd("SYSTEM.attach")
    

dbg.cmd("Break")

dbg.cmd(f"Data.LOAD.Elf {elfpath} /CPP")

dbg.cmd("SYStem.JtagClock 50MHz")

dbg.cmd("Trace.METHOD ONCHIP")
dbg.cmd("ETM.Trace ON")
dbg.cmd("ETM.ON")

dbg.cmd("Trace.arm")

dbg.cmd("Go")

