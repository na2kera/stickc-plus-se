#pragma once
#include <cstdint>
namespace mini { namespace cfg {
constexpr uint32_t debounceMs = 20, abortMs = 800, idleMs = 30000;
constexpr uint32_t countdownMs = 1500, pitchFeedbackMs = 600;
constexpr uint32_t memoryShowMs = 450, memoryBlankMs = 150, memoryInputMs = 3000;
constexpr uint32_t balloonMs = 15000, flightMs = 20000, calibrationMs = 2000;
constexpr uint32_t micStaleMs = 500, micRate = 16000;
constexpr unsigned micSamples = 256;
constexpr float alpha = .2f, minCalibrationGap = 30.f;
constexpr int width = 240, height = 135, fieldTop = 26, fieldBottom = 111;
constexpr float flightX = 45.f, flightRadius = 7.f, obstacleWidth = 18.f;
constexpr float gapHeight = 70.f, obstacleSpeed = 60.f;
constexpr uint32_t obstacleIntervalMs = 2000;
constexpr uint8_t brightness[3] = {64, 128, 220};
} }
