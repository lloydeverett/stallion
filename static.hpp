#pragma once

#include "view.hpp"

class StaticListView : public ListView {
private:
    std::string _title;
    std::vector<std::shared_ptr<View>> _items;
public:
    StaticListView(std::string title, std::vector<std::shared_ptr<View>> items) : _title(std::move(title)), _items(std::move(items)) { }
    virtual std::vector<std::shared_ptr<View>> items() override {
        return _items;
    }
    virtual std::string title() override {
        return _title;
    }
};

class StaticRootView : public RootView {
private:
    std::string _title;
    std::vector<std::shared_ptr<View>> _tabs;
public:
    StaticRootView(std::string title, std::vector<std::shared_ptr<View>> tabs) : _title(std::move(title)), _tabs(std::move(tabs)) { }
    virtual std::vector<std::shared_ptr<View>> tabs() override {
        return _tabs;
    }
    virtual std::string title() override {
        return _title;
    }
};

