/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "keyboard.h"
#include "../hal_config.h"
#include <Arduino.h>
#include <mooncake_log.h>

static const std::string _tag = "Keyboard";

static volatile bool _isr_flag = false;

static void IRAM_ATTR gpio_isr_handler()
{
    _isr_flag = true;
}

bool Keyboard::init()
{
    mclog::tagInfo(_tag, "init");

    _tca8418 = new Adafruit_TCA8418();
    auto ret = _tca8418->begin();
    if (!ret) {
        mclog::tagError(_tag, "init tca8418 failed");
        return false;
    }

    _tca8418->matrix(7, 8);
    _tca8418->flush();

    pinMode(HAL_PIN_KEYBOARD_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(HAL_PIN_KEYBOARD_INT), gpio_isr_handler, FALLING);

    _tca8418->enableInterrupts();

    return true;
}

void Keyboard::update()
{
    clearKeyEvent();

    if (!_isr_flag) {
        return;
    }

    _key_event_raw_buffer = get_key_event_raw(_tca8418->getEvent());

    //  try to clear the IRQ flag
    //  if there are pending events it is not cleared
    _tca8418->writeRegister8(TCA8418_REG_INT_STAT, 1);
    int intstat = _tca8418->readRegister8(TCA8418_REG_INT_STAT);
    if ((intstat & 0x01) == 0) {
        _isr_flag = false;
    }

    remap(_key_event_raw_buffer);
    onKeyEventRaw.emit(_key_event_raw_buffer);

    update_modifier_mask(_key_event_raw_buffer);
    _key_event_buffer = convertToKeyEvent(_key_event_raw_buffer);
    bool fn_active = isFnActive();
    onKeyEvent.emit(_key_event_buffer);
}

Keyboard::KeyEventRaw_t Keyboard::get_key_event_raw(const uint8_t& eventRaw)
{
    KeyEventRaw_t ret;
    ret.state       = eventRaw & 0x80;
    uint16_t buffer = eventRaw;
    buffer &= 0x7F;
    buffer--;
    ret.row = buffer / 10;
    ret.col = buffer % 10;
    return ret;
}

// Remap to the same as cardputer
void Keyboard::remap(KeyEventRaw_t& key)
{
    // Col
    uint8_t col = 0;
    col         = key.row * 2;
    if (key.col > 3) col++;

    // Row
    uint8_t row = 0;
    row         = (key.col + 4) % 4;

    key.row = row;
    key.col = col;
}

void Keyboard::update_modifier_mask(const KeyEventRaw_t& key)
{
    // Read physical Fn state from TCA8418 (covers race where Fn press hasn't been processed yet)
    // Fn remapped location is row=2,col=0. Original matrix coords: orig_row = col/2, orig_col = (col%2==0 ? row : row+4).
    // For Fn at remapped (2,0) => orig_row = 0, orig_col = 2
    if (_tca8418) {
        uint8_t fn_orig_row = 0;
        uint8_t fn_orig_col = 2;
        uint8_t fn_row_pin = fn_orig_row; // TCA8418_ROW0 ..
        uint8_t fn_col_pin = TCA8418_COL0 + fn_orig_col; // TCA8418_COL2 ..
        uint8_t fn_row_val = _tca8418->digitalRead(fn_row_pin);
        uint8_t fn_col_val = _tca8418->digitalRead(fn_col_pin);
        bool fn_phys_pressed = (fn_row_val == TCA8418_LOW) || (fn_col_val == TCA8418_LOW);
        if (fn_phys_pressed) {
            _fn_pressed = true;
            _fn_last_state = true;
            _fn_last_ts = millis();
        } else {
            // do not clear _fn_pressed/_fn_last_state here to avoid races where
            // physical read lags other events; explicit Fn release will clear state
        }
    }

    // Fn key (user marked): remapped location observed at row=2,col=0
    if (key.row == 2 && key.col == 0) {
        _fn_pressed = key.state;
        _fn_last_state = key.state;
        _fn_last_ts = millis();
        return; // Fn does not act as a normal modifier
    }

    // Check control key (row 4 pos 0 => index row=3 col=0)
    if (key.row == 3 && key.col == 0) {
        if (key.state) {
            _modifier_mask |= KEY_MOD_LCTRL;
        } else {
            _modifier_mask &= ~KEY_MOD_LCTRL;
        }
    }

    // Shift keys: keep Aa at (row=2,col=1) as shift
    if (key.row == 2 && key.col == 1) {
        if (key.state) {
            _modifier_mask |= KEY_MOD_LSHIFT;
        } else {
            _modifier_mask &= ~KEY_MOD_LSHIFT;
        }
    }

    // Windows / Meta / Opt key: row 4 pos 1 => row=3,col=1
    if (key.row == 3 && key.col == 1) {
        if (key.state) {
            _modifier_mask |= KEY_MOD_LMETA;
        } else {
            _modifier_mask &= ~KEY_MOD_LMETA;
        }
    }

    // If multiple shift positions exist, add them here (e.g., right shift)
    // (row=2,col=0 is Fn now and not treated as shift)
}

struct KeyValue_t {
    const char* firstName;
    const KeScanCode_t firstKeyCode;
    const char* secondName;
    const KeScanCode_t secondKeyCode;
};

const KeyValue_t _key_value_map[4][14] = {{{"`", KEY_GRAVE, "~", KEY_GRAVE},
                                           {"1", KEY_1, "!", KEY_1},
                                           {"2", KEY_2, "@", KEY_2},
                                           {"3", KEY_3, "#", KEY_3},
                                           {"4", KEY_4, "$", KEY_4},
                                           {"5", KEY_5, "%", KEY_5},
                                           {"6", KEY_6, "^", KEY_6},
                                           {"7", KEY_7, "&", KEY_7},
                                           {"8", KEY_8, "*", KEY_8},
                                           {"9", KEY_9, "(", KEY_9},
                                           {"0", KEY_0, ")", KEY_0},
                                           {"-", KEY_MINUS, "_", KEY_MINUS},
                                           {"=", KEY_EQUAL, "+", KEY_EQUAL},
                                           {"del", KEY_BACKSPACE, "del", KEY_BACKSPACE}},
                                          {{"tab", KEY_TAB, "tab", KEY_TAB},
                                           {"q", KEY_Q, "Q", KEY_Q},
                                           {"w", KEY_W, "W", KEY_W},
                                           {"e", KEY_E, "E", KEY_E},
                                           {"r", KEY_R, "R", KEY_R},
                                           {"t", KEY_T, "T", KEY_T},
                                           {"y", KEY_Y, "Y", KEY_Y},
                                           {"u", KEY_U, "U", KEY_U},
                                           {"i", KEY_I, "I", KEY_I},
                                           {"o", KEY_O, "O", KEY_O},
                                           {"p", KEY_P, "P", KEY_P},
                                           {"[", KEY_LEFTBRACE, "{", KEY_LEFTBRACE},
                                           {"]", KEY_RIGHTBRACE, "}", KEY_RIGHTBRACE},
                                           {"\\", KEY_BACKSLASH, "|", KEY_BACKSLASH}},
                                          {{"shift", KEY_LEFTSHIFT, "shift", KEY_LEFTSHIFT},
                                           {"shift", KEY_LEFTSHIFT, "shift", KEY_LEFTSHIFT},
                                           {"a", KEY_A, "A", KEY_A},
                                           {"s", KEY_S, "S", KEY_S},
                                           {"d", KEY_D, "D", KEY_D},
                                           {"f", KEY_F, "F", KEY_F},
                                           {"g", KEY_G, "G", KEY_G},
                                           {"h", KEY_H, "H", KEY_H},
                                           {"j", KEY_J, "J", KEY_J},
                                           {"k", KEY_K, "K", KEY_K},
                                           {"l", KEY_L, "L", KEY_L},
                                           {";", KEY_SEMICOLON, ":", KEY_SEMICOLON},
                                           {"'", KEY_APOSTROPHE, "\"", KEY_APOSTROPHE},
                                           {"enter", KEY_ENTER, "enter", KEY_ENTER}},
                                          {{"ctrl", KEY_LEFTCTRL, "ctrl", KEY_LEFTCTRL},
                                           {"opt", KEY_LEFTMETA, "opt", KEY_LEFTMETA},
                                           {"alt", KEY_LEFTALT, "alt", KEY_LEFTALT},
                                           {"z", KEY_Z, "Z", KEY_Z},
                                           {"x", KEY_X, "X", KEY_X},
                                           {"c", KEY_C, "C", KEY_C},
                                           {"v", KEY_V, "V", KEY_V},
                                           {"b", KEY_B, "B", KEY_B},
                                           {"n", KEY_N, "N", KEY_N},
                                           {"m", KEY_M, "M", KEY_M},
                                           {",", KEY_COMMA, "<", KEY_COMMA},
                                           {".", KEY_DOT, ">", KEY_DOT},
                                           {"/", KEY_SLASH, "?", KEY_SLASH},
                                           {" ", KEY_SPACE, " ", KEY_SPACE}}};

Keyboard::KeyEvent_t Keyboard::convertToKeyEvent(const KeyEventRaw_t& key)
{
    KeyEvent_t ret;

    ret.state = key.state;

    // If this is the Fn key itself, always emit no key (modifier only)
    // This ensures release events are not treated as Shift
    if (key.row == 2 && key.col == 0) {
        ret.keyCode = KEY_NONE;
        ret.keyName = "Fn";
        ret.isModifier = false;
        return ret;
    }

    // Fn-layer overrides (treat recent Fn press within short window as active to avoid race conditions)
    bool fn_active = isFnActive();
    if (fn_active) {        // If Fn is held, map special keys as requested
        // row/col are zero-based indices in _key_value_map
        if (key.row == 3 && key.col == 0) { // row 4 pos 0 => ESC
            ret.keyCode = KEY_ESC;
            ret.keyName = "Esc";
            ret.isModifier = false;
            return ret;
        }
        if (key.row == 3 && key.col == 13) { // row 4 pos 13 => Delete
            ret.keyCode = KEY_DELETE;
            ret.keyName = "Del";
            ret.isModifier = false;
            return ret;
        }
        // Observed physical positions for arrows (mapped to ; . , / etc.)
        if (key.row == 3 && key.col == 10) { // observed -> Left
            ret.keyCode = KEY_LEFT;
            ret.keyName = "Left";
            ret.isModifier = false;
            return ret;
        }
        if (key.row == 3 && key.col == 11) { // observed -> Down
            ret.keyCode = KEY_DOWN;
            ret.keyName = "Down";
            ret.isModifier = false;
            return ret;
        }
        if (key.row == 3 && key.col == 12) { // observed -> Right
            ret.keyCode = KEY_RIGHT;
            ret.keyName = "Right";
            ret.isModifier = false;
            return ret;
        }
        if (key.row == 2 && key.col == 11) { // observed -> Up
            ret.keyCode = KEY_UP;
            ret.keyName = "Up";
            ret.isModifier = false;
            return ret;
        }
        // If Fn held and key itself is the Fn key, emit no key (modifier only)
        if (key.row == 2 && key.col == 0) {
            ret.keyCode = KEY_NONE;
            ret.keyName = "Fn";
            ret.isModifier = false;
            return ret;
        }
    }

    // For letters: use shift/caps lock to determine case
    // For special characters: use shift to determine which symbol
    bool use_shifted_version = false;

    // Check if this is a letter key (a-z)
    KeScanCode_t baseKeyCode = _key_value_map[key.row][key.col].firstKeyCode;
    bool isLetter            = (baseKeyCode >= KEY_A && baseKeyCode <= KEY_Z);

    if (isLetter) {
        // For letters, use shift or caps lock (if set by other means)
        use_shifted_version = (_modifier_mask & KEY_MOD_LSHIFT) || _is_capslock_locked;
    } else {
        // For non-letters (numbers, symbols), only use shift
        use_shifted_version = (_modifier_mask & KEY_MOD_LSHIFT);
    }

    if (use_shifted_version) {
        ret.keyCode = _key_value_map[key.row][key.col].secondKeyCode;
        ret.keyName = _key_value_map[key.row][key.col].secondName;
    } else {
        ret.keyCode = _key_value_map[key.row][key.col].firstKeyCode;
        ret.keyName = _key_value_map[key.row][key.col].firstName;
    }

    if (ret.keyCode == KEY_LEFTSHIFT || ret.keyCode == KEY_LEFTCTRL || ret.keyCode == KEY_LEFTMETA) {
        ret.isModifier = true;
    } else {
        ret.isModifier = false;
    }

    return ret;
}

void Keyboard::clearKeyEvent()
{
    _key_event_raw_buffer.state = false;
    _key_event_raw_buffer.row   = 233;
    _key_event_raw_buffer.col   = 233;
    _key_event_buffer.keyCode   = KEY_NONE;
}
