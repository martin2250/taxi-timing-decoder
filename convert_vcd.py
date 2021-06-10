#!/usr/bin/python

vcd = open('test.vcd')

# read header
while not '$timescale' in vcd.readline():
    pass
timescale = vcd.readline().strip()
if timescale != '1fs':
    raise ValueError('timescale not 1fs')
while True:
    line = vcd.readline().strip()
    l1, l2 = '$var  1 ', ' o_timing $end'
    if line.startswith(l1) and line.endswith(l2):
        break
char = line[len(l1)]
while not '$dumpvars' in vcd.readline():
    pass
# read vcd
time = 0
data: list[tuple[int, bool]] = []#
while line := vcd.readline().strip():
    if line.startswith('#'):
        time = int(line[1:])
    if line.endswith(char):
        data.append((
            time,
            line[0] == '1',
        ))
# process data
samplerate = 625e6
timescale = 1e-12
state_last = 0
sample_last = 0
with open('test.vcd.txt', 'w') as f_out:
    for time, state in data:
        # convert to actual time
        time = (time // 1000) * timescale
        sample = int(samplerate * time)
        for _ in range(sample_last, sample):
            print(state_last, file=f_out)
        state_last = 250 if state else 0
        sample_last = sample
