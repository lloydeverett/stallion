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
    const std::string name;
    const std::optional<Keycode> default_keycode;
    const std::function<void(CommandRuntime& command_runtime)> execute;
    Command(std::string name, std::function<void(CommandRuntime& command_runtime)> execute) : name(std::move(name)), default_keycode(std::nullopt), execute(std::move(execute)) {}
    Command(std::string name, Keycode default_keycode, std::function<void(CommandRuntime& command_runtime)> execute) : name(std::move(name)), default_keycode(default_keycode), execute(std::move(execute)) {}
};

class CommandSet {
public:
    std::optional<std::string> title;
    std::vector<Command> commands;
    CommandSet(std::optional<std::string> title, std::vector<Command> commands) : title(std::move(title)), commands(std::move(commands)) {}
};

class View {
public:
    virtual std::string title() = 0;
    virtual ftxui::Component renderer() = 0;
    virtual std::shared_ptr<View> active_child() {
        return nullptr;
    }
    virtual CommandSet commands() {
        return { std::nullopt, { } };
    };
    virtual ~View() = default;
};

class MultiCommandSet {
private:
    struct KeycodeMapEntry {
        const CommandSet* command_set { nullptr };
        const Command* command { nullptr };
    };
    std::array<KeycodeMapEntry, keycode_to_integral(Keycode::MAX) + 1> keycode_map;
    static std::vector<CommandSet> init_command_set_elements(View& root_view) {
        std::vector<CommandSet> result;
        result.push_back(root_view.commands());
        for (auto view { root_view.active_child() }; view; view = view->active_child()) {
            result.push_back(view->commands());
        }
        return result;
    }
public:
    const std::vector<CommandSet> command_sets;
    MultiCommandSet(View& view) : command_sets(init_command_set_elements(view)) {
        for (const CommandSet& command_set : command_sets) {
            for (const Command& command : command_set.commands) {
                if (command.default_keycode) {
                    keycode_map[keycode_to_integral(*command.default_keycode)] = KeycodeMapEntry { &command_set, &command };
                }
            }
        }
    }
    const Command* lookup_keypress(Keycode keycode) const {
        KeycodeMapEntry entry = keycode_map[keycode_to_integral(keycode)];
        if (entry.command_set != nullptr || entry.command != nullptr) {
            assert(entry.command_set != nullptr && entry.command != nullptr);
            return entry.command;
        }
        return nullptr;
    }
    bool handle_keypress(Keycode keycode, CommandRuntime& command_runtime) {
        const Command* command = lookup_keypress(keycode);
        if (command != nullptr) {
            command->execute(command_runtime);
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
    std::optional<MultiCommandSet> multi_command_set;
    boost::asio::any_io_executor executor;
    boost::asio::steady_timer timer;
    std::optional<Toast> toast_value;
    RootViewCommandRuntime command_runtime;

    static ftxui::Element render_help(const MultiCommandSet& multi_command_set) {
        ftxui::Elements shortcut_elements;
        for (std::size_t i = 0; i < multi_command_set.command_sets.size(); i++) {
            const CommandSet& command_set { multi_command_set.command_sets[i] };
            if (command_set.title) {
                shortcut_elements.push_back(ftxui::text(*command_set.title + " ") | color(ftxui::Color::Cyan));
            }
            for (const Command& command : command_set.commands) {
                if (command.default_keycode && multi_command_set.lookup_keypress(*command.default_keycode) == &command) {
                    shortcut_elements.push_back(ftxui::text(std::string() + keycode_to_char(*command.default_keycode) + " ") | ftxui::dim);
                    shortcut_elements.push_back(ftxui::text(command.name + " ") | color(ftxui::Color::GrayDark));
                }
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
    Command quit_command { "quit", [&](CommandRuntime& rt) { app.Exit(); } } ;
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
    virtual CommandSet commands() override {
        return { "App", {
            { "quit", Keycode::Q, [&](CommandRuntime& rt) { app.Exit(); } },
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
            multi_command_set.emplace(*this);

            return ftxui::vbox({
                tab_menu->Render(),
                tab_content->Render() | ftxui::flex,
                ftxui::separator() | color(ftxui::Color::GrayDark),
                (toast_value ? render_toast(*toast_value) : render_help(*multi_command_set)) | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 1)
            });
        }) };

        return ftxui::CatchEvent(renderer, [&](ftxui::Event event) {
            for (std::size_t i = 0; i < keycode_events.size(); i++) {
                if (event == keycode_events[i]) {
                    if (log_file) {
                        log_file << "keypress: " << keycode_to_char(integral_to_keycode(i)) << std::endl;
                    }
                    return multi_command_set->handle_keypress(integral_to_keycode(i), command_runtime);
                }
            }
            return false;
        });
    }
};

