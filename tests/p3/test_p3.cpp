// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

TEST(reg_empty_and_null) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(!r.add(nullptr));
    CHECK(r.find("absent") == nullptr);
    CHECK(r.size() == 0);
    int visits = 0;
    r.for_each([&](const Tool&) { ++visits; });
    CHECK(visits == 0);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    for (int i = 0; i < 20; ++i)
        REQUIRE(r.add(std::make_unique<NamedTool>(std::to_string(i))));
    std::vector<int> visits(20, 0);
    r.for_each([&](const Tool& tool) {
        const auto i = std::stoul(tool.name());
        REQUIRE(i < visits.size());
        ++visits[i];
        CHECK(r.find(tool.name()) == &tool);
    });
    for (int n : visits) CHECK(n == 1);
}

TEST(reg_10000_tools_and_alternative_hash) {
    ToolRegistry r;
    Tool* first = nullptr;
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        if (i == 0) first = r.find("tool_0");
        CHECK(r.size() == static_cast<std::size_t>(i + 1));
        CHECK(r.load_factor() <= 1.0);
        CHECK(r.load_factor() == static_cast<double>(r.size()) / r.bucket_count());
    }
    CHECK(r.find("tool_0") == first);
    std::vector<int> weak(r.bucket_count(), 0), real(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        REQUIRE(r.find(name) != nullptr);
        CHECK(r.find(name)->name() == name);
        ++weak[(static_cast<unsigned char>(name[0]) + name.size()) % weak.size()];
        ++real[ToolRegistry::hash(name) % real.size()];
    }
    int weak_max = 0, real_max = 0;
    for (int n : weak) if (n > weak_max) weak_max = n;
    for (int n : real) if (n > real_max) real_max = n;
    CHECK(r.max_probe_length() == static_cast<std::size_t>(real_max));
    CHECK(r.max_probe_length() <= 64);
    std::printf("buckets=%zu load=%.6f longest=%zu weak_longest=%d\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), weak_max);
}

TEST(reg_collision_and_resize_boundary) {
    ToolRegistry r;
    const auto initial = r.bucket_count();
    std::vector<std::string> names;
    for (int i = 0; names.size() < 3; ++i) {
        std::string name = "collision_" + std::to_string(i);
        if (r.hash(name) % initial == 0) names.push_back(name);
    }
    for (const auto& name : names) REQUIRE(r.add(std::make_unique<NamedTool>(name)));
    CHECK(r.max_probe_length() == 3);
    for (const auto& name : names) REQUIRE(r.find(name) != nullptr);
    for (std::size_t i = names.size(); i < initial; ++i)
        REQUIRE(r.add(std::make_unique<NamedTool>("fill_" + std::to_string(i))));
    CHECK(r.bucket_count() == initial);
    CHECK(r.load_factor() == 1.0);
    CHECK(!r.add(nullptr));
    CHECK(!r.add(std::make_unique<NamedTool>(names[0])));
    CHECK(r.size() == initial);
    CHECK(r.bucket_count() == initial);
    REQUIRE(r.add(std::make_unique<NamedTool>("trigger_resize")));
    CHECK(r.bucket_count() == initial * 2);
    for (const auto& name : names) REQUIRE(r.find(name) != nullptr);
}

TEST(ws_normal) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes/todo.txt");
}

TEST(ws_normalizes) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes/todo.txt");
}

TEST(ws_dot) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
}

TEST(ws_internal_parent) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");
}

TEST(ws_parent_cancels) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());
}

TEST(ws_nonexistent) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new/dir/file.txt");
}

TEST(ws_dots_in_name) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_rejects_empty) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(!reason.empty());
    CHECK(reason == "empty path");
}

TEST(ws_rejects_nul) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve(std::string("notes") + '\0' + "/x", &out, &reason));
    CHECK(!reason.empty());

}

TEST(ws_rejects_absolute) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());

}

TEST(ws_rejects_dotdot) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("..", &out, &reason));
    CHECK(!reason.empty());

}

TEST(ws_rejects_parent) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());

}

TEST(ws_rejects_nested_escape) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("a/../../b", &out, &reason));
    CHECK(!reason.empty());

}

TEST(ws_rejects_reentry) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve("notes/../../ws/notes/todo.txt", &out, &reason));
    CHECK(!reason.empty());

}

TEST(ws_rejects_absolute_inside) {
    TempDir d; Workspace ws(d.path()); fs::path out; std::string reason;
    CHECK(!ws.resolve(ws.root().string() + "/inside", &out, &reason));
    CHECK(!reason.empty());

}

TEST(calc_subtract_left) {
    double value = 0; std::string error;
    REQUIRE(eval("10-4-3", &value, &error));
    CHECK(value == 3);
}

TEST(calc_divide_left) {
    double value = 0; std::string error;
    REQUIRE(eval("100/10/5", &value, &error));
    CHECK(value == 2);
}

TEST(calc_parentheses) {
    double value = 0; std::string error;
    REQUIRE(eval("(2+3)*4", &value, &error));
    CHECK(value == 20);
}

TEST(calc_mixed) {
    double value = 0; std::string error;
    REQUIRE(eval("2*(3+4)-5", &value, &error));
    CHECK(value == 9);
}

TEST(calc_negatives) {
    double value = 0; std::string error;
    REQUIRE(eval("-3*-3", &value, &error));
    CHECK(value == 9);
}

TEST(calc_fraction) {
    double value = 0; std::string error;
    REQUIRE(eval("7/2", &value, &error));
    CHECK(value == 3.5);
}

TEST(calc_decimal_extra) {
    double value = 0; std::string error;
    REQUIRE(eval("1.5+2.25*2", &value, &error));
    CHECK(value == 6);
}

TEST(calc_negative_extra) {
    double value = 0; std::string error;
    REQUIRE(eval("2*(-3+1)", &value, &error));
    CHECK(value == -4);
}

TEST(calc_computed_zero) {
    double value = 0; std::string error;
    CHECK(!eval("1/(2-2)", &value, &error));
    CHECK(error == "division by zero");
}
TEST(calc_large_inputs) {
    double value = 0; std::string error;
    std::string expression = "1";
    for (int i = 1; i < 20000; ++i) expression += "+1";
    REQUIRE(eval(expression, &value, &error)); CHECK(value == 20000);
    REQUIRE(eval(std::string(5000, '(') + "7" + std::string(5000, ')'), &value, &error));
    CHECK(value == 7);
}
TEST(calc_malformed_tokens) {
    using K = CalcToken::Kind;
    const std::vector<std::vector<CalcToken>> bad = {
        {}, {{K::Plus, 0}}, {{K::Number, 1}, {K::Plus, 0}},
        {{K::RParen, 0}}, {{K::LParen, 0}, {K::Number, 1}},
        {{K::Number, 1}, {K::Number, 2}}, {{K::LParen, 0}, {K::RParen, 0}}
    };
    for (const auto& tokens : bad) {
        double value = 0; std::string error;
        CHECK(!calc_evaluate(tokens, &value, &error)); CHECK(!error.empty());
    }
}
int main(int argc, char** argv) { return mt::run_all(argc, argv); }
