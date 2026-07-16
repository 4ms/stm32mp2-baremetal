import sys
import lauterbach.trace32.rcl as t32
from pathlib import Path

dbg = t32.connect()
# Dir where we're calling the script from, e.g. an example project dir
basepath = Path.cwd()
elfpath = basepath / "build" / "main.elf"

dbg.print(f"Flashing via python rcl {elfpath}...")

# Attaching right after a target reset races the DAP / debug-power domain coming
# back up on the MP25, so the first attach usually fails. Reset once, then retry
# the attach a few times (reconfiguring each try) so one `make flash-t32` is
# enough -- no manual re-run.

dbg.cmd("reset")

try:
    dbg.cmd("system.RESETOUT")
except Exception:
    print("First resetout failed")
    dbg.cmd("wait 0.5")
    dbg.cmd("SYSTEM.reset")
    dbg.cmd("system.RESETOUT")

dbg.cmd("wait 2.0")


def configure():
    dbg.cmd("SYStem.CPU STM32MP257F-CA35")
    dbg.cmd("SYStem.config DEBUGPORTTYPE SWD ")
    dbg.cmd("SYStem.JtagClock 50MHz")
    dbg.cmd("Trace.DISable")
    dbg.cmd("SYStem.MemAccess DAP")
    dbg.cmd("system.Option DUALPORT on")


configure()

ATTEMPTS = 6
attached = False
for i in range(1, ATTEMPTS + 1):
    try:
        dbg.cmd("SYSTEM.attach")
        attached = True
        print(f"attached on attempt {i}")
        break
    except Exception as e:
        print(f"attach {i}/{ATTEMPTS} failed: {e}")
        dbg.cmd("wait 0.1")

if not attached:
    print("ERROR: could not attach to the target after retries")
    sys.exit(1)

dbg.cmd("Break")

dbg.cmd(f"Data.LOAD.Elf {elfpath} /CPP")

dbg.cmd("Trace.METHOD ONCHIP")
dbg.cmd("ETM.Trace ON")
dbg.cmd("ETM.ON")

dbg.cmd("Trace.arm")

dbg.cmd("Go")
dbg.print("running")
