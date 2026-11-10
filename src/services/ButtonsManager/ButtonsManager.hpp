#pragma once

#include "config.h"
#include "Button.hpp"
#include "Event.hpp"

class ButtonsManager {
public:
    ButtonsManager(GPIO_TypeDef *portS1, uint8_t pinS1,
                   GPIO_TypeDef *portS2, uint8_t pinS2,
                   GPIO_TypeDef *portS3, uint8_t pinS3,
                   GPIO_TypeDef *portS4, uint8_t pinS4)
            : btnS1(portS1, pinS1),
              btnS2(portS2, pinS2),
              btnS3(portS3, pinS3),
              btnS4(portS4, pinS4) {}

    void poll(EventQueue &queue) {
        handle(btnS1, EventType::ButtonS1, releaseSeen[0], lastRelease[0], queue);
        handle(btnS2, EventType::ButtonS2, releaseSeen[1], lastRelease[1], queue);
        handle(btnS3, EventType::ButtonS3, releaseSeen[2], lastRelease[2], queue);
        handle(btnS4, EventType::ButtonS4, releaseSeen[3], lastRelease[3], queue);

        checkCombination(queue);
    }

private:
    static constexpr uint32_t DoubleClickGap = BUTTONS_DOUBLE_CLICK_GAP_MS; // мс
    bool releaseSeen[4] = {};
    uint32_t lastRelease[4] = {};
    bool comboArmed = false;
    bool longComboSent = false;
    uint32_t comboStart = 0;

    Button<> btnS1, btnS2, btnS3, btnS4;

    void handle(Button<> &b, EventType type, bool &seen, uint32_t &lastR, EventQueue &queue) {
        const auto now = RccDriver::GetMsTicks();
        auto e = b.tick();

        if (e == Button<>::Event::Pressed) {
            queue.push({type, 0});     // short press start
        } else if (e == Button<>::Event::Held) {
            queue.push({type, 1});     // hold
        } else if (e == Button<>::Event::Released) {
            queue.push({type, 2});
            // Пара отпусканий ближе DoubleClickGap. seen отделён от метки времени:
            // lastR == 0 — это и «ещё не было», и тик 0, поэтому нулём гасить нельзя.
            if (seen && static_cast<uint32_t>(now - lastR) <= DoubleClickGap) {
                queue.push({type, 3}); // double click code
                seen = false;
            } else {
                seen = true;
                lastR = now;
            }
        }
    }

    void checkCombination(EventQueue &queue) {
        const bool both = btnS1.stablePressed() && btnS2.stablePressed();
        const uint32_t now = RccDriver::GetMsTicks();

        if (both) {
            if (!comboArmed) {
                comboArmed = true;
                comboStart = now;
                longComboSent = false;
            } else if (!longComboSent &&
                       static_cast<uint32_t>(now - comboStart) > BUTTONS_COMBO_LONG_THRESHOLD_MS) {
                queue.push({EventType::ButtonS1, 10}); // combo long, один раз за удержание
                longComboSent = true;
            }
        } else if (comboArmed) {
            if (!longComboSent &&
                static_cast<uint32_t>(now - comboStart) < BUTTONS_COMBO_LONG_THRESHOLD_MS) {
                queue.push({EventType::ButtonS1, 9}); // short combo
            }
            comboArmed = false;
            longComboSent = false;
        }
    }
};