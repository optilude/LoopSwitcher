"""Hardware test for preset-management against the real EEPROM.

Flash first:  .venv/bin/pio run -e eeprom_demo -t upload
Run:          .venv/bin/python tools/verify_eeprom.py
Opening the serial port resets the board, which stands in for a power cycle. The script
wipes the EEPROM, so do not run it on a unit whose labels and presets you want to keep.
Close any serial monitor first.
"""
import glob, sys, time
import serial

PORT = glob.glob('/dev/cu.usbmodem*')[0]


def boot(read_s=3.6):
    """Open the port (this resets the board) and return what it prints while booting."""
    s = serial.Serial(PORT, 115200, timeout=0.2)
    end = time.time() + read_s
    out = b''
    while time.time() < end:
        out += s.read(256)
    return s, out.decode(errors='replace')


def cmd(s, line, wait=0.4):
    s.write((line + '\n').encode())
    end = time.time() + wait
    out = b''
    while time.time() < end:
        out += s.read(256)
    return out.decode(errors='replace').strip()


failures = []


def check(name, cond, detail=''):
    print(('PASS  ' if cond else 'FAIL  ') + name + ('' if cond else '   -> ' + detail))
    if not cond:
        failures.append(name)


# 1. fresh upload
s, text = boot()
print('--- boot after upload:\n' + text)
check('EEPROM length is 256', 'EEPROM length: 256' in text, text)

# 2. wipe, so the following steps start from a known blank EEPROM
r = cmd(s, 'w', 2.0)
check('wipe then begin() reports defaults', 'defaults loaded' in r, r)
d = cmd(s, 'd', 0.8)
check('blank store has default labels', 'loop 1: Loop 1' in d and 'loop 8: Loop 8' in d, d)
check('blank store has no presets', all(('preset %d: (empty)' % i) in d for i in range(1, 9)), d)
check('blank store has the default state', 'saved state: mask 0x0 manual mode, preset 1' in d, d)

# 3. write data
for line in ['l 1 TS808', 'l 8 DELAY', 'p 1 05 CLEAN', 'p 3 FF LEAD', 'p 8 00 BYPASS']:
    r = cmd(s, line)
    check('"%s" accepted' % line, r == 'ok', r)
check('invalid name rejected', cmd(s, 'l 2 ELEVENCHARS') == 'rejected')
check('rename on an empty slot rejected', cmd(s, 'n 2 X') == 'rejected')
check('empty preset name rejected', cmd(s, 'p 2 01') == 'rejected')
cmd(s, 's 5A 1 2')
check('flush works', cmd(s, 'f') == 'flushed')
s.close()

EXPECTED = ['loop 1: TS808', 'loop 8: DELAY', 'preset 1: CLEAN  mask 0x5', 'preset 3: LEAD  mask 0xFF',
            'preset 8: BYPASS  mask 0x0', 'saved state: mask 0x5A preset mode, preset 3']


def verify(text, label):
    ok = 'stored data is valid' in text and all(e in text for e in EXPECTED)
    check(label, ok, text)
    return ok


# 4. 20 power cycles
all_ok = True
for i in range(1, 21):
    s, text = boot()
    all_ok &= verify(text, 'power cycle %2d restores labels, presets and state' % i)
    s.close()

# 4b. re-flash the same firmware: does the upload keep the EEPROM?
import subprocess
r = subprocess.run(['/Users/maraspeli/Build/Daisy/LoopSwitcher/.venv/bin/pio', 'run', '-e', 'eeprom_demo', '-t', 'upload'],
                   cwd='/Users/maraspeli/Build/Daisy/LoopSwitcher', capture_output=True, text=True)
check('re-upload succeeded', r.returncode == 0, r.stdout[-400:])
s, text = boot()
verify(text, 're-uploading the firmware keeps the stored data')
s.close()

# 5. idle save: no flush, wait for the 2 s delay
s, _ = boot()
cmd(s, 's 3C 0 4')
time.sleep(3.0)
s.close()
s, text = boot()
check('idle save restores state after a reset', 'saved state: mask 0x3C manual mode, preset 5' in text, text)

# 6. a change followed by a reset inside the idle delay is not saved (accepted behaviour)
cmd(s, 's 11 1 0')
s.close()
s, text = boot()
check('a state change reset before the delay is not saved', 'saved state: mask 0x3C manual mode, preset 5' in text, text)

# 7. more than 20 saves, so the ring wraps twice
last = None
for i in range(45):
    mask = (i * 7 + 1) & 0xFF
    last = (mask, i & 1, i & 7)
    cmd(s, 's %X %d %d' % last, 0.15)
    cmd(s, 'f', 0.15)
s.close()
s, text = boot()
expected = 'saved state: mask 0x%X %s, preset %d' % (last[0], 'preset mode' if last[1] else 'manual mode', last[2] + 1)
check('after 45 saves the newest record is restored', expected in text, expected + '\n' + text)
check('configuration untouched by the state saves', all(e in text for e in EXPECTED[:5]), text)

# 8. delete + wipe
check('delete accepted', cmd(s, 'x 3') == 'ok')
check('second delete rejected', cmd(s, 'x 3') == 'rejected')
s.close()
s, text = boot()
check('delete survives a reset', 'preset 3: (empty)' in text and 'preset 1: CLEAN' in text, text)

cmd(s, 'w', 2.0)
s.close()
s, text = boot()
check('after wipe and begin() the next boot finds valid default data',
      'stored data is valid' in text and 'loop 1: Loop 1' in text and 'preset 1: (empty)' in text, text)
s.close()

print()
print('RESULT:', 'ALL CHECKS PASSED' if not failures else 'FAILED: ' + ', '.join(failures))
sys.exit(1 if failures else 0)
