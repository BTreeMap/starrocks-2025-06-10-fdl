#pragma once

#include "http/http_handler.h"

namespace starrocks {

class BfdParser;

class HeapAction : public HttpHandler {
public:
    HeapAction() = default;
    ~HeapAction() override = default;

    void handle(HttpRequest* req) override;
};

class GrowthAction : public HttpHandler {
public:
    GrowthAction() = default;
    ~GrowthAction() override = default;

    void handle(HttpRequest* req) override;
};

class ProfileAction : public HttpHandler {
public:
    ProfileAction() = default;
    ~ProfileAction() override = default;

    void handle(HttpRequest* req) override;
};

class IOProfileAction : public HttpHandler {
public:
    IOProfileAction() = default;
    ~IOProfileAction() override = default;

    void handle(HttpRequest* req) override;
};

class PmuProfileAction : public HttpHandler {
public:
    PmuProfileAction() = default;
    ~PmuProfileAction() override = default;
    void handle(HttpRequest* req) override {}
};

class ContentionAction : public HttpHandler {
public:
    ContentionAction() = default;
    ~ContentionAction() override = default;

    void handle(HttpRequest* req) override {}
};

class CmdlineAction : public HttpHandler {
public:
    CmdlineAction() = default;
    ~CmdlineAction() override = default;
    void handle(HttpRequest* req) override;
};

class SymbolAction : public HttpHandler {
public:
    SymbolAction(BfdParser* parser) : _parser(parser) {}
    ~SymbolAction() override = default;

    void handle(HttpRequest* req) override;

private:
    BfdParser* _parser;
};
} // namespace starrocks
