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
std::vector<std::tuple<int, uint64_t>> decode_taxi_time(
    std::vector<uint16_t> trace, double bit_length, size_t num_bits,
    uint16_t threshold) {
    const double frame_gap = bit_length * 9.5;  // gap between frames
    size_t index_start = 0;                     // start index of current frame
    std::vector<std::tuple<int, uint64_t>> frames = {};
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
        for (size_t i_bit = 0; i_bit < num_bits; i_bit++) {
            // skip synchronization bits (every eight bit)
            size_t i_bit_eff = i_bit + i_bit / 8;
            // sample frame between edges, starting at the first data bit
            double index_offset =
                (static_cast<double>(i_bit_eff) + 1.5) * bit_length;
            size_t i_sample = index_start + static_cast<size_t>(index_offset);
            // check bounds
            if (i_sample > trace.size()) {
                goto end_of_trace;
            }
            // sample
            code <<= 1;
            if (trace[i_sample] > threshold) {
                code |= 1;
            }
        }
        frames.push_back({index_start, code});
    }
end_of_trace:
    return frames;
}

// only for testing
#include <fstream>
int main(int argc, char **argv) {
    double interval = 1.6000000213622911e-09;
    double bit_length = 1 / 25e6 / interval;
    std::vector<uint16_t> data;
    // load data from file
    std::ifstream infile("test.txt");
    uint16_t value;
    while (infile >> value) {
        data.push_back(value);
    }
    // decode
    auto frames = decode_taxi_time(data, bit_length, 35, 128);
    // print
    for (auto frame : frames) {
        size_t index = std::get<0>(frame);
        uint64_t code = std::get<1>(frame);

        uint64_t whole = code >> 19;
        uint64_t fractional = code & ((1 << 19) - 1);
        double time =
            static_cast<double>(whole) + static_cast<double>(fractional) * 2e-6;
        std::cout << index << " " << std::fixed << std::setprecision(6)
                    << time << std::endl;
    }
}
