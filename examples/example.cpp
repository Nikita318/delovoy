// example.cpp — пример использования DiscreteInput.
// Сборка: g++ -std=c++17 -Wall -Wextra example.cpp -o example

#include "discrete_input.hpp"
#include <cstdio>

using namespace std::chrono_literals;

int main() {
    DiscreteInput::Config cfg;
    cfg.inverted    = false;
    cfg.debounceOn  = 30ms;
    cfg.debounceOff = 50ms;

    DiscreteInput button(cfg);

    button.onRising([](bool, DiscreteInput::TimePoint t) {
        std::printf("[%4lld ms] нажата\n", static_cast<long long>(t.count()));
    });
    button.onFalling([&button](bool, DiscreteInput::TimePoint t) {
        std::printf("[%4lld ms] отпущена, импульс %lld ms\n",
                    static_cast<long long>(t.count()),
                    static_cast<long long>(button.previousStateDuration().count()));
    });

    // Имитация входа с дребезгом, опрос каждые 5 мс.
    auto rawAt = [](long ms) {
        if (ms >= 100 && ms < 120) return (ms / 5) % 2 == 0; // дребезг при нажатии
        if (ms >= 120 && ms < 400) return true;              // нажато
        if (ms >= 400 && ms < 415) return (ms / 5) % 2 == 1; // дребезг при отпускании
        if (ms >= 600 && ms < 610) return true;              // короткая помеха — отфильтруется
        return false;
    };

    bool longPressReported = false;
    for (long ms = 0; ms <= 800; ms += 5) {
        button.update(rawAt(ms), DiscreteInput::TimePoint{ms});

        if (button.heldOnFor(200ms) && !longPressReported) {
            std::printf("[%4ld ms] долгое нажатие\n", ms);
            longPressReported = true;
        }
        if (button.falling()) longPressReported = false;
    }

    std::printf("Нажатий: %u, отпусканий: %u\n",
                button.risingCount(), button.fallingCount());
    return 0;
}
