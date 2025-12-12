#pragma once

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
        handle(btnS1, EventType::ButtonS1, lastRelease[0], queue);
        handle(btnS2, EventType::ButtonS2, lastRelease[1], queue);
        handle(btnS3, EventType::ButtonS3, lastRelease[2], queue);
        handle(btnS4, EventType::ButtonS4, lastRelease[3], queue);

        checkCombination(queue);
    }

private:
    static constexpr uint32_t DoubleClickGap = 250; // мс
    uint32_t lastRelease[4] = {0, 0, 0, 0};

    Button<> btnS1, btnS2, btnS3, btnS4;

    void handle(Button<> &b, EventType type, uint32_t &lastR, EventQueue &queue) {
        const auto now = RccDriver::GetMsTicks();
        auto e = b.tick();

        if (e == Button<>::Event::Pressed) {
            queue.push({type, 0});     // short press start
            lastR = 0;
        } else if (e == Button<>::Event::Held) {
            queue.push({type, 1});     // hold
        } else if (e == Button<>::Event::Released) {
            queue.push({type, 2});
            if (now - lastR <= DoubleClickGap) {
                queue.push({type, 3}); // double click code
            }
            lastR = now;
        }
    }

    void checkCombination(EventQueue &queue) {
        const bool s1 = !btnS1.Read();
        const bool s2 = !btnS2.Read();
        const uint32_t now = RccDriver::GetMsTicks();

        static uint32_t comboStart = 0;

        if (s1 && s2) {
            if (comboStart == 0) comboStart = now;

            if (now - comboStart > 400) { // long combo
                queue.push({EventType::ButtonS1, 10}); // combo long
                comboStart = 0;
            }
        } else {
            if (comboStart != 0 && (now - comboStart < 400)) {
                queue.push({EventType::ButtonS1, 9}); // short combo
            }
            comboStart = 0;
        }
    }
};