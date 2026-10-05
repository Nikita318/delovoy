// discrete_input.hpp
// Объект обработки дискретного входного сигнала (C++17, header-only).
//
// Возможности:
//   - инверсия входа (NO / NC контакт);
//   - антидребезг: раздельные задержки на включение и на выключение;
//   - детектирование фронтов (rising / falling), флаги живут один цикл update();
//   - время нахождения в текущем состоянии;
//   - счётчик срабатываний;
//   - колбэки на изменение состояния;
//   - принудительное значение (force) для отладки / наладки.
//
// Время передаётся снаружи в update(raw, now), поэтому класс не зависит
// от платформы: подходит и для ПК, и для микроконтроллера (millis()).

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

class DiscreteInput {
public:
    using Duration  = std::chrono::milliseconds;
    using TimePoint = Duration;  // монотонное время "с момента старта"
    using Callback  = std::function<void(bool newState, TimePoint now)>;

    struct Config {
        bool     inverted    = false;        // инвертировать сырой сигнал
        Duration debounceOn  = Duration{20}; // сигнал должен держаться 1 столько, чтобы стать 1
        Duration debounceOff = Duration{20}; // сигнал должен держаться 0 столько, чтобы стать 0
        bool     initialState = false;       // состояние до первого update()
    };

    DiscreteInput() : DiscreteInput(Config{}) {}
    explicit DiscreteInput(const Config& cfg)
        : cfg_(cfg), state_(cfg.initialState), candidate_(cfg.initialState) {}

    // Главный вызов: подать сырое значение входа и текущее время.
    // Вызывать периодически (в цикле опроса / прерывании таймера).
    void update(bool raw, TimePoint now) {
        rising_ = falling_ = false;

        if (!started_) {
            started_      = true;
            stateSince_   = now;
            candidateSince_ = now;
        }

        bool value = cfg_.inverted ? !raw : raw;
        if (forced_) value = *forced_;
        lastFiltered_ = value;

        if (value != candidate_) {          // начался новый переход — перезапуск таймера
            candidate_      = value;
            candidateSince_ = now;
        }

        if (candidate_ != state_) {
            const Duration need = candidate_ ? cfg_.debounceOn : cfg_.debounceOff;
            if (forced_ || now - candidateSince_ >= need) {
                applyState(candidate_, now);
            }
        }
        lastUpdate_ = now;
    }

    // --- Состояние ---
    bool state()   const { return state_; }       // отфильтрованное значение
    bool isOn()    const { return state_; }
    bool isOff()   const { return !state_; }
    bool rising()  const { return rising_; }      // фронт 0->1 в последнем update()
    bool falling() const { return falling_; }     // фронт 1->0 в последнем update()
    bool changed() const { return rising_ || falling_; }
    bool raw()     const { return lastFiltered_; } // после инверсии/force, до антидребезга

    // Сколько времени вход находится в текущем состоянии.
    Duration stateDuration() const { return started_ ? lastUpdate_ - stateSince_ : Duration{0}; }
    // Удобные проверки "держится дольше чем..."
    bool heldOnFor(Duration d)  const { return state_  && stateDuration() >= d; }
    bool heldOffFor(Duration d) const { return !state_ && stateDuration() >= d; }
    // Длительность предыдущего состояния (например, длина импульса после falling()).
    Duration previousStateDuration() const { return prevDuration_; }

    std::uint32_t risingCount()  const { return risingCount_; }
    std::uint32_t fallingCount() const { return fallingCount_; }
    void resetCounters() { risingCount_ = fallingCount_ = 0; }

    // --- Колбэки ---
    void onChange(Callback cb)  { onChange_  = std::move(cb); }
    void onRising(Callback cb)  { onRising_  = std::move(cb); }
    void onFalling(Callback cb) { onFalling_ = std::move(cb); }

    // --- Принудительное значение (минует антидребезг) ---
    void force(bool value)  { forced_ = value; }
    void releaseForce()     { forced_.reset(); }
    bool isForced() const   { return forced_.has_value(); }

    // --- Настройка ---
    const Config& config() const { return cfg_; }
    void setInverted(bool v)       { cfg_.inverted = v; }
    void setDebounce(Duration on, Duration off) { cfg_.debounceOn = on; cfg_.debounceOff = off; }

    // Сброс в начальное состояние (без вызова колбэков).
    void reset(bool state = false) {
        state_ = candidate_ = lastFiltered_ = state;
        rising_ = falling_ = started_ = false;
        prevDuration_ = Duration{0};
        resetCounters();
    }

private:
    void applyState(bool s, TimePoint now) {
        prevDuration_ = now - stateSince_;
        state_      = s;
        stateSince_ = now;
        if (s) { rising_ = true;  ++risingCount_;  if (onRising_)  onRising_(s, now); }
        else   { falling_ = true; ++fallingCount_; if (onFalling_) onFalling_(s, now); }
        if (onChange_) onChange_(s, now);
    }

    Config cfg_;

    bool state_;
    bool candidate_;
    bool lastFiltered_ = false;
    bool rising_  = false;
    bool falling_ = false;
    bool started_ = false;
    std::optional<bool> forced_;

    TimePoint stateSince_{0};
    TimePoint candidateSince_{0};
    TimePoint lastUpdate_{0};
    Duration  prevDuration_{0};

    std::uint32_t risingCount_  = 0;
    std::uint32_t fallingCount_ = 0;

    Callback onChange_, onRising_, onFalling_;
};
