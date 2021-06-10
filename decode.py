#!/usr/bin/python
import scipy.io
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

def decode_channel(chan: np.ndarray, interval: float, baud: float = 25e6, num_bits: int = 35):
    # timing
    bit = 1 / baud / interval # 25 MHz baud rate
    frame_gap = 8.5 * bit
    # threshold and edge detection
    chan = chan > 0
    chan_edge = np.logical_xor(chan, np.roll(chan, 1))
    # decoder state
    frames = []
    # go through data
    index_start = 0
    while True:
        last_edge_index = index_start
        for i in range(index_start, len(chan)):
            if not chan_edge[i]:
                continue
            if (i - last_edge_index) < frame_gap:
                last_edge_index = i
                continue
            index_start = i
            break
        else:
            break
        code = 0
        samples = []
        for i_bit in range(num_bits):
            i_bit_eff = i_bit + (i_bit // 8)
            i_bit_eff = index_start + int((i_bit_eff + 1.5) * bit)
            if i_bit_eff >= len(chan):
                break # prevents outer else from running -> breaks outer loop
            samples.append(i_bit_eff)
            code |= chan[i_bit_eff] << (num_bits - 1 - i_bit)
        else:
            sec_of_day = code >> 19
            sub_second = code & ((1 << 19) - 1)
            frame_time = sec_of_day + sub_second * 2e-6
            frames.append((index_start, frame_time))
            # print(f'sec: 0x{sec_of_day:X} subsec: 0x{sub_second:X}')
            # print(f'frame at {index_start}: {frame_time:0.6f}')
            continue
        break
    return frames

last_time = 0
def decode_mat(p: Path) -> tuple[np.ndarray, float]:
    global last_time
    mat = scipy.io.loadmat(p)
    # channels = []
    interval = mat['Tinterval'][0, 0]
    for name in ('A', 'B', 'C', 'D'):
        if name not in mat:
            continue
        frames = decode_channel(mat[name][:,0], interval)
        if len(frames) == 0:
            print('NO FRAMES', last_time)
            continue
        last_time = frames[0][1]

for i in range(512):
    # decode_mat(f'data/triggered/triggered_{i+1:03}.mat')
    decode_mat(f'data/untriggered/untriggered_{i+1:03}.mat')
    # decode_mat(f'data/untriggered_rapid/untriggered_rapid_{i+1:03}.mat')


mat = scipy.io.loadmat('data/triggered/triggered_001.mat')
interval = mat['Tinterval'][0, 0]
data = mat['A'][:,0]

print('interval', interval)
with open('test.txt', 'w') as f:
    for x in data:
        print(int(255 * (x + 1)/2), file=f)
