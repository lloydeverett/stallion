#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <array>
#include <cassert>

#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

#include "log.hpp"

enum class Keycode {
    // omit H, J, K, L to allow for vim-like navigation
    MIN = 0,
    A = Keycode::MIN,
    B,
    C,
    D,
    E,
    F,
    G,
    I,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    MAX = Keycode::Z,
};

constexpr char keycode_to_char(Keycode keycode) {
    switch (keycode) {
        case Keycode::A: return 'a';
        case Keycode::B: return 'b';
        case Keycode::C: return 'c';
        case Keycode::D: return 'd';
        case Keycode::E: return 'e';
        case Keycode::F: return 'f';
        case Keycode::G: return 'g';
        case Keycode::I: return 'i';
        case Keycode::M: return 'm';
        case Keycode::N: return 'n';
        case Keycode::O: return 'o';
        case Keycode::P: return 'p';
        case Keycode::Q: return 'q';
        case Keycode::R: return 'r';
        case Keycode::S: return 's';
        case Keycode::T: return 't';
        case Keycode::U: return 'u';
        case Keycode::V: return 'v';
        case Keycode::W: return 'w';
        case Keycode::X: return 'x';
        case Keycode::Y: return 'y';
        case Keycode::Z: return 'z';
    }
}

constexpr std::size_t keycode_to_integral(Keycode keycode) {
    return static_cast<std::size_t>(keycode) - static_cast<std::size_t>(Keycode::MIN);
}

constexpr Keycode integral_to_keycode(std::size_t integral) {
    return static_cast<Keycode>(integral + static_cast<std::size_t>(Keycode::MIN));
}

inline const std::array<ftxui::Event, keycode_to_integral(Keycode::MAX) + 1> keycode_events {
    ftxui::Event::Character(keycode_to_char(Keycode::A)),
    ftxui::Event::Character(keycode_to_char(Keycode::B)),
    ftxui::Event::Character(keycode_to_char(Keycode::C)),
    ftxui::Event::Character(keycode_to_char(Keycode::D)),
    ftxui::Event::Character(keycode_to_char(Keycode::E)),
    ftxui::Event::Character(keycode_to_char(Keycode::F)),
    ftxui::Event::Character(keycode_to_char(Keycode::G)),
    ftxui::Event::Character(keycode_to_char(Keycode::I)),
    ftxui::Event::Character(keycode_to_char(Keycode::M)),
    ftxui::Event::Character(keycode_to_char(Keycode::N)),
    ftxui::Event::Character(keycode_to_char(Keycode::O)),
    ftxui::Event::Character(keycode_to_char(Keycode::P)),
    ftxui::Event::Character(keycode_to_char(Keycode::Q)),
    ftxui::Event::Character(keycode_to_char(Keycode::R)),
    ftxui::Event::Character(keycode_to_char(Keycode::S)),
    ftxui::Event::Character(keycode_to_char(Keycode::T)),
    ftxui::Event::Character(keycode_to_char(Keycode::U)),
    ftxui::Event::Character(keycode_to_char(Keycode::V)),
    ftxui::Event::Character(keycode_to_char(Keycode::W)),
    ftxui::Event::Character(keycode_to_char(Keycode::X)),
    ftxui::Event::Character(keycode_to_char(Keycode::Y)),
    ftxui::Event::Character(keycode_to_char(Keycode::Z)),
};

class CommandRuntime {
public:
    virtual void info(std::string_view text) = 0;
    virtual void error_warn(std::string_view text) = 0;
    virtual void error_fail(std::string_view text) = 0;
    virtual ~CommandRuntime() = default;
};

class Command {
public:
    std::string name;
    std::function<void(CommandRuntime& command_runtime)> execute;
    Command(std::string name, std::function<void(CommandRuntime& command_runtime)> execute) : name(std::move(name)), execute(std::move(execute)) {}
};

class Shortcut {
public:
    Keycode keycode;
    Command command;
    Shortcut(Keycode keycode, Command command) : keycode(std::move(keycode)), command(std::move(command)) {}
};

class Help {
public:
    const std::optional<std::string> title;
    const std::vector<Shortcut> shortcuts;
    Help(std::optional<std::string> title, std::vector<Shortcut> shortcuts) : title(std::move(title)), shortcuts(std::move(shortcuts)) { }
    Help(std::optional<std::string> title, std::initializer_list<Shortcut> shortcuts) : title(std::move(title)), shortcuts(std::move(shortcuts)) { }
};

class View {
public:
    virtual std::string title() = 0;
    virtual ftxui::Component renderer() = 0;
    virtual std::shared_ptr<View> active_child() {
        return nullptr;
    }
    virtual Help help() {
        return { std::nullopt, { } };
    };
    virtual ~View() = default;
};

class ConsolidatedHelp {
private:
    struct KeycodeMapEntry {
        const Help* help { nullptr };
        const Shortcut* shortcut { nullptr };
    };
    std::array<KeycodeMapEntry, keycode_to_integral(Keycode::MAX) + 1> keycode_map;
    static std::vector<Help> init_help_elements(View& root_view) {
        std::vector<Help> result;
        result.push_back(root_view.help());
        for (auto view { root_view.active_child() }; view; view = view->active_child()) {
            result.push_back(view->help());
        }
        return result;
    }
public:
    const std::vector<Help> help_elements;
    ConsolidatedHelp(View& view) : help_elements(init_help_elements(view)) {
        for (const Help& help : help_elements) {
            for (const Shortcut& shortcut : help.shortcuts) {
                keycode_map[keycode_to_integral(shortcut.keycode)] = KeycodeMapEntry { &help, &shortcut };
            }
        }
    }
    bool handle_keypress(Keycode keycode, CommandRuntime& command_runtime) {
        KeycodeMapEntry entry = keycode_map[keycode_to_integral(keycode)];

        if (entry.help != nullptr || entry.shortcut != nullptr) {
            assert(entry.help != nullptr && entry.shortcut != nullptr);
            entry.shortcut->command.execute(command_runtime);
            return true;
        }

        return false;
    }
};

class ListView : public View {
private:
    int item_index = 0;
    std::vector<std::shared_ptr<View>> items_value;
    std::vector<std::string> item_entries;
    ftxui::Component item_menu;
    ftxui::Components item_content_components;
    ftxui::Component item_content;
    ftxui::Component container;
public:
    virtual std::vector<std::shared_ptr<View>> items() = 0;
    virtual std::shared_ptr<View> active_child() override {
        if (item_index >= 0 && item_index < items_value.size()) {
            return items_value[item_index];
        }
        return nullptr;
    }
    virtual ftxui::Component renderer() override {
        items_value = items();

        item_entries.reserve(items_value.size());
        for (auto item : items_value) {
            item_entries.push_back(item->title());
        }

        item_content_components.reserve(items_value.size());
        for (auto item : items_value) {
            item_content_components.push_back(item->renderer());
        }
        item_content = ftxui::Container::Tab(item_content_components, &item_index);

        item_menu = Menu(&item_entries, &item_index, ftxui::MenuOption::Vertical());

        container = ftxui::Container::Horizontal({
                item_menu,
                item_content,
        });

        return ftxui::Renderer(container, [&] {
            return ftxui::hbox({
                item_menu->Render() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
                ftxui::filler() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 1),
                item_content->Render() | ftxui::flex,
            });
        });
    }
};

class RootView : public View {
public:
    enum class ToastVariant {
        INFO,
        ERROR_WARN,
        ERROR_FAIL
    };
    class Toast {
    public:
        const std::string message;
        const ToastVariant variant;
        Toast(std::string message, ToastVariant variant) : message(message), variant(variant) {}
    };
private:
    class RootViewCommandRuntime : public CommandRuntime {
        RootView& view;
    public:
        RootViewCommandRuntime(RootView& view) : view(view) { }
        virtual void info(std::string_view text) override {
            view.toast({ std::string(text), ToastVariant::INFO });
            if (log_file) {
                log_file << "info: " << text << std::endl;
            }
        }
        virtual void error_warn(std::string_view text) override {
            view.toast({ std::string(text), ToastVariant::ERROR_WARN });
            if (log_file) {
                log_file << "error_warn: " << text << std::endl;
            }
        }
        virtual void error_fail(std::string_view text) override {
            view.toast({ std::string(text), ToastVariant::ERROR_FAIL });
            if (log_file) {
                log_file << "error_fail: " << text << std::endl;
            }
        }
    };

    int tab_index = 0;
    ftxui::App& app;
    std::vector<std::shared_ptr<View>> tabs_value;
    std::vector<std::string> tab_entries;
    ftxui::Component tab_menu;
    ftxui::Components tab_content_components;
    ftxui::Component tab_content;
    ftxui::Component container;
    std::optional<ConsolidatedHelp> consolidated_help;
    boost::asio::any_io_executor executor;
    boost::asio::steady_timer timer;
    std::optional<Toast> toast_value;
    RootViewCommandRuntime command_runtime;

    static ftxui::Element render_consolidated_help(const ConsolidatedHelp& consolidated_help) {
        ftxui::Elements shortcut_elements;
        for (std::size_t i = 0; i < consolidated_help.help_elements.size(); i++) {
            const Help& help { consolidated_help.help_elements[i] };
            if (help.title) {
                shortcut_elements.push_back(ftxui::text(*help.title + " ") | color(ftxui::Color::Cyan));
            }
            for (const Shortcut& shortcut : help.shortcuts) {
                shortcut_elements.push_back(ftxui::text(std::string() + keycode_to_char(shortcut.keycode) + " ") | ftxui::dim);
                shortcut_elements.push_back(ftxui::text(shortcut.command.name + " ") | color(ftxui::Color::GrayDark));
            }
        }
        return ftxui::hbox(shortcut_elements);
    }
    static ftxui::Element render_toast(const Toast& toast_value) {
        ftxui::Element element { ftxui::text(toast_value.message) };
        switch (toast_value.variant) {
            case ToastVariant::INFO:
                return element;
            case ToastVariant::ERROR_WARN:
                return element | color(ftxui::Color::DarkOrange);
            case ToastVariant::ERROR_FAIL:
                return element | color(ftxui::Color::IndianRed1);
        }
    }
public:
    explicit RootView(ftxui::App& app, boost::asio::any_io_executor executor) : app(app), executor(executor), timer(executor), command_runtime(*this) { }
    virtual std::vector<std::shared_ptr<View>> tabs() = 0;
    virtual std::shared_ptr<View> active_child() override {
        if (tab_index >= 0 && tab_index < tabs_value.size()) {
            return tabs_value[tab_index];
        }
        return nullptr;
    }
    virtual std::chrono::milliseconds toast_duration() {
        return std::chrono::milliseconds(2000);
    }
    virtual void toast(const Toast& toast) {
        toast_value.emplace(toast);
        timer.expires_after(toast_duration());
        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec) {
                app.Post([&] {
                    toast_value.reset();
                    app.PostEvent(ftxui::Event::Custom);
                });
            }
        });
    }
    virtual Help help() override {
        return { "App", {
            { Keycode::Q, { "quit", [&](CommandRuntime& rt) { app.Exit(); } } },
        } };
    }
    virtual ftxui::Component renderer() override {
        tabs_value = tabs();

        tab_entries.reserve(tabs_value.size());
        for (auto tab : tabs_value) {
            tab_entries.push_back(tab->title());
        }

        tab_content_components.reserve(tabs_value.size());
        for (auto tab : tabs_value) {
            tab_content_components.push_back(tab->renderer());
        }
        tab_content = ftxui::Container::Tab(tab_content_components, &tab_index);

        tab_menu = Menu(&tab_entries, &tab_index, ftxui::MenuOption::HorizontalAnimated());

        container = ftxui::Container::Vertical({
                tab_menu,
                tab_content,
        });

        auto renderer { ftxui::Renderer(container, [&] {
            consolidated_help.emplace(*this);

            return ftxui::vbox({
                tab_menu->Render(),
                tab_content->Render() | ftxui::flex,
                ftxui::separator() | color(ftxui::Color::GrayDark),
                (toast_value ? render_toast(*toast_value) : render_consolidated_help(*consolidated_help)) | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 1)
            });
        }) };

        return ftxui::CatchEvent(renderer, [&](ftxui::Event event) {
            for (std::size_t i = 0; i < keycode_events.size(); i++) {
                if (event == keycode_events[i]) {
                    if (log_file) {
                        log_file << "keypress: " << keycode_to_char(integral_to_keycode(i)) << std::endl;
                    }
                    return consolidated_help->handle_keypress(integral_to_keycode(i), command_runtime);
                }
            }
            return false;
        });
    }
};

