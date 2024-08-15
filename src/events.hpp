#pragma once

#include <milg/event.hpp>

#include <cstdint>

namespace milg {
    class WindowCloseEvent : public Event {
    public:
        DEFINE_EVENT_CLASS_TYPE(WindowClose)

        WindowCloseEvent() = default;
    };

    class WindowResizeEvent : public Event {
    public:
        DEFINE_EVENT_CLASS_TYPE(WindowResize)

        WindowResizeEvent(uint32_t width, uint32_t height) : m_width(width), m_height(height) {
        }

        uint32_t width() const {
            return m_width;
        }

        uint32_t height() const {
            return m_height;
        }

    private:
        uint32_t m_width;
        uint32_t m_height;
    };

    class KeyEvent : public Event {
    public:
        inline int32_t key_code() const {
            return m_key_code;
        }

    protected:
        KeyEvent(int32_t key_code) : m_key_code(key_code) {
        }

        int32_t m_key_code;
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        DEFINE_EVENT_CLASS_TYPE(KeyPressed)

        KeyPressedEvent(int32_t key_code, int32_t repeat_count) : KeyEvent(key_code), m_repeat_count(repeat_count) {
        }

        inline int32_t repeat_count() const {
            return m_repeat_count;
        }

    private:
        int32_t m_repeat_count;
    };

    class KeyReleasedEvent : public KeyEvent {
    public:
        DEFINE_EVENT_CLASS_TYPE(KeyReleased)

        KeyReleasedEvent(int32_t key_code) : KeyEvent(key_code) {
        }
    };

    class KeyTypedEvent : public KeyEvent {
    public:
        DEFINE_EVENT_CLASS_TYPE(KeyTyped)

        KeyTypedEvent(int32_t key_code) : KeyEvent(key_code) {
        }
    };

    class MouseMovedEvent : public Event {
    public:
        DEFINE_EVENT_CLASS_TYPE(MouseMoved)

        MouseMovedEvent(float x, float y) : m_x(x), m_y(y) {
        }

        float x() const {
            return m_x;
        }

        float y() const {
            return m_y;
        }

    private:
        float m_x;
        float m_y;
    };

    class MouseScrolledEvent : public Event {
    public:
        DEFINE_EVENT_CLASS_TYPE(MouseScrolled)

        MouseScrolledEvent(float x_offset, float y_offset) : m_x_offset(x_offset), m_y_offset(y_offset) {
        }

        float x_offset() const {
            return m_x_offset;
        }

        float y_offset() const {
            return m_y_offset;
        }

    private:
        float m_x_offset;
        float m_y_offset;
    };

    class MouseButtonEvent : public Event {
    public:
        inline int32_t button() const {
            return m_button;
        }

    protected:
        MouseButtonEvent(int32_t button) : m_button(button) {
        }

        int32_t m_button;
    };

    class MousePressedEvent : public MouseButtonEvent {
    public:
        DEFINE_EVENT_CLASS_TYPE(MouseButtonPressed)

        MousePressedEvent(int32_t button) : MouseButtonEvent(button) {
        }
    };

    class MouseReleasedEvent : public MouseButtonEvent {
    public:
        DEFINE_EVENT_CLASS_TYPE(MouseButtonReleased)

        MouseReleasedEvent(int32_t button) : MouseButtonEvent(button) {
        }
    };

    class RawEvent : public Event {
    public:
        DEFINE_EVENT_CLASS_TYPE(Raw)

        RawEvent(void *raw_event) : m_raw_event(raw_event) {
        }

        void *raw_event() const {
            return m_raw_event;
        }

    private:
        void *m_raw_event;
    };
} // namespace milg
