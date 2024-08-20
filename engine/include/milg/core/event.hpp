#pragma once

#define DEFINE_EVENT_CLASS_TYPE(type)                                                                                  \
    static EventType get_static_type() {                                                                               \
        return EventType::type;                                                                                        \
    }                                                                                                                  \
    virtual EventType get_event_type() const override {                                                                \
        return get_static_type();                                                                                      \
    }

#define BIND_EVENT_FN(fn)                                                                                              \
    [this](auto &&...args) -> decltype(auto) {                                                                         \
        return this->fn(std::forward<decltype(args)>(args)...);                                                        \
    }

namespace milg {
    enum class EventType {
        WindowClose,
        WindowResize,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled,
        Raw
    };

    class Event {
    public:
        virtual ~Event() = default;

        virtual EventType get_event_type() const = 0;

        bool handled = false;
    };

    class EventDispatcher {
    public:
        EventDispatcher(Event &event) : event(event) {
        }

        template <typename T, typename F> bool dispatch(const F &func) {
            if (event.get_event_type() == T::get_static_type()) {
                event.handled |= func(static_cast<T &>(event));
                return true;
            }
            return false;
        }

    private:
        Event &event;
    };
} // namespace milg
