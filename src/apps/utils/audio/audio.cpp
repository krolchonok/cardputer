/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <random>

namespace audio {
static constexpr uint8_t k_sfx_channel         = 7;
static constexpr int32_t k_default_sfx_volume  = 90;
static constexpr const char* k_setting_sfx_en  = "kbd_sfx_en";
static constexpr const char* k_setting_sfx_vol = "kbd_sfx_vol";

static bool g_sfx_enabled   = true;
static uint8_t g_sfx_volume = k_default_sfx_volume;

static void apply_sfx_volume()
{
    GetHAL().speaker.setChannelVolume(k_sfx_channel, g_sfx_volume);
}

static std::vector<int> c_major_scale = {60, 62, 64, 65, 67, 69, 71};  // C大调音阶（C D E F G A B）

void play_tone(int frequency, double durationSec)
{
    if (!g_sfx_enabled || g_sfx_volume == 0) {
        return;
    }

    if (GetHAL().speaker.getVolume() <= 0) {
        return;
    }

    apply_sfx_volume();

    const int sample_rate = GetHAL().speaker.config().sample_rate;
    const int samples     = static_cast<int>(sample_rate * durationSec);
    std::vector<int16_t> buffer(samples * 2);  // 双声道

    const int fade_len    = 200;  // 淡出长度（采样点）
    const float amplitude = 32767.0f / 5;

    for (int i = 0; i < samples; ++i) {
        float amp = amplitude;

        // 应用结尾淡出（fade-out）
        if (i >= samples - fade_len) {
            float fade_factor = static_cast<float>(samples - i) / fade_len;
            amp *= fade_factor;
        }

        int16_t value     = static_cast<int16_t>(amp * sin(2.0 * M_PI * frequency * i / sample_rate));
        buffer[i * 2]     = value;  // 左声道
        buffer[i * 2 + 1] = value;  // 右声道
    }

    GetHAL().speaker.playRaw(buffer.data(), buffer.size(), sample_rate, false, 1, k_sfx_channel, true);
}

void play_melody(const std::vector<int>& midiList, double durationSec = 0.1)
{
    if (!g_sfx_enabled || g_sfx_volume == 0) {
        return;
    }

    if (GetHAL().speaker.getVolume() <= 0) {
        return;
    }

    apply_sfx_volume();

    const int sample_rate      = GetHAL().speaker.config().sample_rate;
    const int samples_per_note = static_cast<int>(sample_rate * durationSec);
    const int fade_len         = 200;  // 每个音符结尾的淡出长度
    const float amplitude      = 32767.0f / 5;

    std::vector<int16_t> buffer;                             // 大 buffer 存放整首旋律
    buffer.reserve(midiList.size() * samples_per_note * 2);  // 双声道预留空间

    for (int midiNote : midiList) {
        for (int i = 0; i < samples_per_note; ++i) {
            float amp = amplitude;

            // 应用淡出（仅每个音符的结尾）
            if (i >= samples_per_note - fade_len) {
                float fade_factor = static_cast<float>(samples_per_note - i) / fade_len;
                amp *= fade_factor;
            }

            int16_t sample = 0;
            if (midiNote >= 0) {
                double freq = 440.0 * pow(2.0, (midiNote - 69) / 12.0);
                sample      = static_cast<int16_t>(amp * sin(2.0 * M_PI * freq * i / sample_rate));
            }

            buffer.push_back(sample);  // 左声道
            buffer.push_back(sample);  // 右声道
        }
    }

    GetHAL().speaker.playRaw(buffer.data(), buffer.size(), sample_rate, false, 1, k_sfx_channel, true);
}

void play_tone_from_midi(int midi, double durationSec)
{
    double freq = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    play_tone(static_cast<int>(freq), durationSec);
}

void play_random_tone(int semitoneShift = 0, double durationSec = 0.15)
{
    if (!g_sfx_enabled || g_sfx_volume == 0) {
        return;
    }

    if (GetHAL().speaker.getVolume() <= 0) {
        return;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, static_cast<int>(c_major_scale.size()) - 1);

    int index = dist(gen);
    int midi  = c_major_scale[index] + semitoneShift;

    play_tone_from_midi(midi, durationSec);
}

/* -------------------------------------------------------------------------- */
/*                                  Keyboard                                  */
/* -------------------------------------------------------------------------- */
static void _keyboard_sfx_on_key_event(const Keyboard::KeyEvent_t& event)
{
    if (!event.state) {
        return;
    }

    if (!g_sfx_enabled || g_sfx_volume == 0) {
        return;
    }

    apply_sfx_volume();

    int semitoneShift = 48;
    switch (event.keyCode) {
        case KEY_1:
            play_tone_from_midi(c_major_scale[0] + semitoneShift, 0.02);
            return;
        case KEY_2:
            play_tone_from_midi(c_major_scale[1] + semitoneShift, 0.02);
            return;
        case KEY_3:
            play_tone_from_midi(c_major_scale[2] + semitoneShift, 0.02);
            return;
        case KEY_4:
            play_tone_from_midi(c_major_scale[3] + semitoneShift, 0.02);
            return;
        case KEY_5:
            play_tone_from_midi(c_major_scale[4] + semitoneShift, 0.02);
            return;
        case KEY_6:
            play_tone_from_midi(c_major_scale[5] + semitoneShift, 0.02);
            return;
        case KEY_7:
            play_tone_from_midi(c_major_scale[6] + semitoneShift, 0.02);
            return;
        default:
            play_random_tone(semitoneShift, 0.02);
    }
}

void set_keyboard_sfx_enable(bool enable)
{
    static size_t slot_id  = 0;
    static bool is_enabled = false;

    mclog::tagInfo("audio", "set keyboard sfx enable: {}", enable);
    g_sfx_enabled = enable;
    GetHAL().getSettings().SetBool(k_setting_sfx_en, enable);
    if (enable) {
        if (is_enabled) {
            return;
        }
        is_enabled = true;
        slot_id    = GetHAL().keyboard.onKeyEvent.connect(_keyboard_sfx_on_key_event);
    } else {
        if (!is_enabled) {
            return;
        }
        is_enabled = false;
        GetHAL().keyboard.onKeyEvent.disconnect(slot_id);
    }
}

bool get_keyboard_sfx_enabled()
{
    return g_sfx_enabled;
}

void set_keyboard_sfx_volume(uint8_t volume)
{
    g_sfx_volume = volume;
    apply_sfx_volume();
    GetHAL().getSettings().SetInt(k_setting_sfx_vol, g_sfx_volume);
}

uint8_t get_keyboard_sfx_volume()
{
    return g_sfx_volume;
}

void load_keyboard_sfx_settings()
{
    bool enabled   = GetHAL().getSettings().GetBool(k_setting_sfx_en, true);
    int32_t volume = GetHAL().getSettings().GetInt(k_setting_sfx_vol, k_default_sfx_volume);
    volume         = std::clamp<int32_t>(volume, 0, 255);

    g_sfx_volume = static_cast<uint8_t>(volume);
    apply_sfx_volume();
    set_keyboard_sfx_enable(enabled);
}

}  // namespace audio
