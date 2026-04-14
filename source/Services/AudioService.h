//
// AudioService.h
//

#pragma once

class AudioService {
private:
    double sampleRate_ = 44100.0;
    int blockSize_ = 0;

    double cycleAccumulator_ = 0.0;

    static constexpr int GBA_CLOCK = 16777216;

public:
    void prepare(double sampleRate, int blockSize) {
        sampleRate_ = sampleRate;
        blockSize_ = blockSize;

        cycleAccumulator_ = 0.0;
    }

    int calcCycles(int numSamples) {
        double cycles = (static_cast<double>(numSamples) * GBA_CLOCK) / sampleRate_;
        cycles += cycleAccumulator_;

        int intCycles = static_cast<int>(cycles);
        cycleAccumulator_ = cycles - intCycles;

        return intCycles;
    }
};