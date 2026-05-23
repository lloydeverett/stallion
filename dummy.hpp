#pragma once

#include <random>

#include "view.hpp"

inline int dummy_random() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    return dis(gen);
}

class DummyView : public View {
public:
    int r = dummy_random();
    virtual ftxui::Component renderer() override {
        return ftxui::Renderer([&] {
            return ftxui::text("hello world " + std::to_string(r));
        });
    }
    virtual std::string title() override {
        return "dummy " + std::to_string(r);
    }
    virtual Help help() override {
        return { "View", {
            { Keycode::F, "foo", [] {} }
        } };
    }
};

class DummyListView : public ListView {
public:
    virtual std::vector<std::shared_ptr<View>> items() override {
        return {
            std::make_shared<DummyView>(),
            std::make_shared<DummyView>(),
        };
    }
    virtual std::string title() override {
        return "dummy list " + std::to_string(dummy_random());
    }
};

class NestedDummyListView : public ListView {
    virtual std::vector<std::shared_ptr<View>> items() override {
        return {
            std::make_shared<DummyListView>(),
            std::make_shared<DummyListView>(),
        };
    }
    virtual std::string title() override {
        return "nested dummy list " + std::to_string(dummy_random());
    }
    virtual Help help() override {
        return { "Nested List", {
            { Keycode::X, "xx", [] {} }
        } };
    }
};

class DummyRootView : public RootView {
public:
    virtual std::vector<std::shared_ptr<View>> tabs() override {
        return {
            std::make_shared<NestedDummyListView>(),
            std::make_shared<DummyListView>(),
        };
    }
    virtual std::string title() override {
        return "dummy root " + std::to_string(dummy_random());
    }
    virtual Help help() override {
        return { "Root", {
            { Keycode::R, "hi", [] {} }
        } };
    }
};

