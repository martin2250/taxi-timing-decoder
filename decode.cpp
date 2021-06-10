#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <tuple>
#include <vector>

/**
 * @brief Decode TAXI timestamp signal
 *
 * @param trace oscilloscope trace
 * @param bit_length oscilloscope trace
 * @param num_bits number of bits in a frame
 * @param threshold threshold for high / low
 *
 * @return list of successfully decoded frames, as tuples of <sample_index,
 * frame_data>
 */
std::vector<std::tuple<size_t, uint64_t, size_t>> decode_taxi_time(
    std::vector<uint16_t> trace, double bit_length, size_t num_bits,
    uint16_t threshold) {
    // gap between frames
    const double frame_gap = bit_length * 9.5;
    // sample trace at multiple points to detect errors
    const size_t SAMPLES = 5;
    const double sample_points[SAMPLES] = {0.3, 0.4, 0.5, 0.6, 0.7};
    // start index of current frame
    size_t index_start = 0;
    std::vector<std::tuple<size_t, uint64_t, size_t>> frames = {};
    while (true) {
        // find interframe gap (no edges for > frame_gap)
        size_t last_edge_index = index_start;
        bool state_last = false;
        // loop through array
        for (size_t i = index_start; i < trace.size(); i++) {
            bool state = trace[i] > threshold;
            // edge ?
            if (state == state_last) {
                continue;
            }
            state_last = state;
            // gap between edges long enough?
            if ((i - last_edge_index) < frame_gap) {
                last_edge_index = i;
                continue;
            }
            // frame found, go to decoding step
            index_start = i;
            goto frame_start;
        }
        break;  // loop exited normally -> reached end of array
    frame_start:
        // frame found, decode frame
        uint64_t code = 0;
        size_t num_bit_errors = 0;
        for (size_t i_bit = 0; i_bit < num_bits; i_bit++) {
            // effective bit index, skip start and synchronization bits (every
            // eight bit)
            size_t i_bit_eff = 1 + i_bit + i_bit / 8;
            // count number of samples that exceed threshold
            uint samples_high = 0;
            for (size_t i_sample = 0; i_sample < SAMPLES; i_sample++) {
                // sample bit at <SAMPLES> points, skip start bit
                double index_offset =
                    static_cast<double>(i_bit_eff) + sample_points[i_sample];
                size_t index_sample =
                    index_start +
                    static_cast<size_t>(index_offset * bit_length);
                // check bounds
                if (index_sample > trace.size()) {
                    goto end_of_trace;
                }
                if (trace[index_sample] > threshold) {
                    samples_high += 1;
                }
            }
            // check for errors
            if ((samples_high != 0) && (samples_high != SAMPLES)) {
                num_bit_errors += 1;
            }
            // store sample in code
            code <<= 1;
            if (samples_high > (SAMPLES / 2)) {
                code |= 1;
            }
        }
        frames.push_back({index_start, code, num_bit_errors});
    }
end_of_trace:
    return frames;
}

// simple 4 bit checksum by adding all nibbles and wrapping the carry bit,
// similar to internet checksum
uint64_t checksum_4b(uint64_t v) {
    uint64_t checksum = 0;
    for (int i = 0; i < 16; i++) {
        checksum = (checksum & 0xF) + (v & 0xF) + (checksum >> 4);
        v >>= 4;
    }
    checksum = (checksum & 0xF) + (checksum >> 4);
    return checksum;
}

// only for testing
#include <fstream>
int main(int argc, char **argv) {
    double interval = 1.6000000213622911e-09;
    double baud_rate = 125e6 / 2;
    double bit_length = 1 / baud_rate / interval;
    std::vector<uint16_t> data;
    // load data from file
    std::ifstream infile("test.vcd.txt");
    uint16_t value;
    while (infile >> value) {
        data.push_back(value);
    }
    // decode
    auto frames = decode_taxi_time(data, bit_length, 41, 128);
    // print
    for (auto frame : frames) {
        size_t index = std::get<0>(frame);
        uint64_t code = std::get<1>(frame);
        uint64_t bit_errors = std::get<2>(frame);
        
        if (bit_errors > 0) {
            std::cout << bit_errors << " bit errors" << std::endl;
        }

        uint64_t checksum = code & ((1 << 4) - 1);
        code >>= 4;

        uint64_t checksum_calc = checksum_4b(code);
        if (checksum != checksum_calc) {
            std::cout << "checksum mismatch " << checksum << " "
                      << checksum_calc << std::endl;
        }

        uint64_t fractional = code & ((1 << 20) - 1);
        code >>= 20;
        uint64_t whole = code & ((1 << 17) - 1);
        code >>= 17;

        double time =
            static_cast<double>(whole) + static_cast<double>(fractional) * 1e-6;
        std::cout << index << " " << std::fixed << std::setprecision(6) << time
                  << std::endl;
    }
}
