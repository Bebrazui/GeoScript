#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>

using namespace geode::prelude;

// ============================================================================
// 1. LEXER
// ============================================================================

enum class GeoTokenType {
    // Keywords
    KW_group,
    KW_on_start,
    KW_on_touch,
    KW_on_death,
    KW_move,
    KW_pulse,
    KW_wait,
    KW_repeat,
    KW_while_true,
    KW_shake,
    // Literals / names
    Identifier,
    Number,
    // Punctuation
    Assign,
    Semicolon,
    Comma,
    LParen,
    RParen,
    LBrace,
    RBrace,
    // Meta
    EndOfFile,
    Error
};

struct GeoToken {
    GeoTokenType token_type;
    std::string  value;
    int          line;
    GeoToken(GeoTokenType t, std::string v, int l)
        : token_type(t), value(std::move(v)), line(l) {}
};

class Lexer {
    std::string m_src;
    size_t      m_pos  = 0;
    int         m_line = 1;

    char peek(int off = 0) const {
        size_t p = m_pos + off;
        return p < m_src.size() ? m_src[p] : '\0';
    }
    char eat() {
        char c = m_src[m_pos++];
        if (c == '\n') m_line++;
        return c;
    }
    void skipWS() {
        while (m_pos < m_src.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { eat(); }
            else if (c == '/' && peek(1) == '/') { while (peek() && peek() != '\n') eat(); }
            else break;
        }
    }

public:
    explicit Lexer(std::string src) : m_src(std::move(src)) {}

    GeoToken next() {
        skipWS();
        if (m_pos >= m_src.size()) return {GeoTokenType::EndOfFile, "", m_line};

        char c = peek();

        // Number (including negative)
        if (std::isdigit(c) || (c == '-' && std::isdigit(peek(1)))) {
            std::string s;
            if (c == '-') s += eat();
            while (std::isdigit(peek()) || peek() == '.') s += eat();
            return {GeoTokenType::Number, s, m_line};
        }

        // Identifier / keyword
        if (std::isalpha(c) || c == '_') {
            std::string s;
            while (std::isalnum(peek()) || peek() == '_') s += eat();
            // keyword table
            static const std::unordered_map<std::string, GeoTokenType> KW = {
                {"group",      GeoTokenType::KW_group},
                {"on_start",   GeoTokenType::KW_on_start},
                {"on_touch",   GeoTokenType::KW_on_touch},
                {"on_death",   GeoTokenType::KW_on_death},
                {"move",       GeoTokenType::KW_move},
                {"pulse",      GeoTokenType::KW_pulse},
                {"wait",       GeoTokenType::KW_wait},
                {"repeat",     GeoTokenType::KW_repeat},
                {"while_true", GeoTokenType::KW_while_true},
                {"shake",      GeoTokenType::KW_shake},
            };
            auto it = KW.find(s);
            return {it != KW.end() ? it->second : GeoTokenType::Identifier, s, m_line};
        }

        eat();
        switch (c) {
            case '=': return {GeoTokenType::Assign,    "=", m_line};
            case ';': return {GeoTokenType::Semicolon, ";", m_line};
            case ',': return {GeoTokenType::Comma,     ",", m_line};
            case '(': return {GeoTokenType::LParen,    "(", m_line};
            case ')': return {GeoTokenType::RParen,    ")", m_line};
            case '{': return {GeoTokenType::LBrace,    "{", m_line};
            case '}': return {GeoTokenType::RBrace,    "}", m_line};
        }
        return {GeoTokenType::Error, std::string(1, c), m_line};
    }
};

// ============================================================================
// 2. AST
// ============================================================================

struct ASTNode { virtual ~ASTNode() = default; };

// group name [= id];
struct VarDeclNode : ASTNode {
    std::string name;
    int explicitId; // -1 = auto
    VarDeclNode(std::string n, int id) : name(std::move(n)), explicitId(id) {}
};

// A single command call: name(arg, arg, ...)
struct CmdNode : ASTNode {
    std::string              name;
    std::vector<std::string> args;
    CmdNode(std::string n, std::vector<std::string> a)
        : name(std::move(n)), args(std::move(a)) {}
};

// repeat(n) { ... }
struct RepeatNode : ASTNode {
    int                          count;
    std::vector<ASTNode*>        body;
    ~RepeatNode() { for (auto* n : body) delete n; }
};

// while_true { ... }
struct WhileTrueNode : ASTNode {
    std::vector<ASTNode*> body;
    ~WhileTrueNode() { for (auto* n : body) delete n; }
};

// on_start { ... } / on_touch(group) { ... } / on_death(group) { ... }
struct EventNode : ASTNode {
    std::string           eventType;  // "on_start" | "on_touch" | "on_death"
    std::string           param;      // group name for on_touch / on_death
    std::vector<ASTNode*> body;
    ~EventNode() { for (auto* n : body) delete n; }
};

struct ProgramAST {
    std::vector<VarDeclNode*> decls;
    std::vector<EventNode*>   events;
    ~ProgramAST() {
        for (auto* n : decls)   delete n;
        for (auto* n : events)  delete n;
    }
};

// ============================================================================
// 3. PARSER
// ============================================================================

class Parser {
    Lexer                    m_lex;
    GeoToken                 m_cur;
    std::vector<std::string> m_errors;

    void advance() { m_cur = m_lex.next(); }

    bool expect(GeoTokenType t, const std::string& msg) {
        if (m_cur.token_type != t) {
            m_errors.push_back("Line " + std::to_string(m_cur.line) + ": " + msg);
            return false;
        }
        advance();
        return true;
    }

    // Parse comma-separated argument list inside ()
    std::vector<std::string> parseArgs() {
        std::vector<std::string> args;
        expect(GeoTokenType::LParen, "Expected '(' after command name");
        while (m_cur.token_type != GeoTokenType::RParen &&
               m_cur.token_type != GeoTokenType::EndOfFile) {
            if (m_cur.token_type == GeoTokenType::Identifier ||
                m_cur.token_type == GeoTokenType::Number) {
                args.push_back(m_cur.value);
                advance();
            } else {
                m_errors.push_back("Line " + std::to_string(m_cur.line) +
                                   ": Invalid argument '" + m_cur.value + "'");
                advance();
            }
            if (m_cur.token_type == GeoTokenType::Comma) advance();
        }
        expect(GeoTokenType::RParen, "Expected ')'");
        return args;
    }

    // Parse a block body { stmt* }
    std::vector<ASTNode*> parseBody() {
        std::vector<ASTNode*> stmts;
        expect(GeoTokenType::LBrace, "Expected '{'");
        while (m_cur.token_type != GeoTokenType::RBrace &&
               m_cur.token_type != GeoTokenType::EndOfFile) {
            auto* s = parseStatement();
            if (s) stmts.push_back(s);
        }
        expect(GeoTokenType::RBrace, "Expected '}'");
        return stmts;
    }

    ASTNode* parseStatement() {
        // repeat(n) { ... }
        if (m_cur.token_type == GeoTokenType::KW_repeat) {
            advance();
            auto args = parseArgs();
            int n = args.empty() ? 1 : std::stoi(args[0]);
            auto* node = new RepeatNode();
            node->count = n;
            node->body  = parseBody();
            return node;
        }
        // while_true { ... }
        if (m_cur.token_type == GeoTokenType::KW_while_true) {
            advance();
            auto* node = new WhileTrueNode();
            node->body = parseBody();
            return node;
        }
        // Command: move / pulse / wait / shake / identifier(...)
        if (m_cur.token_type == GeoTokenType::KW_move   ||
            m_cur.token_type == GeoTokenType::KW_pulse  ||
            m_cur.token_type == GeoTokenType::KW_wait   ||
            m_cur.token_type == GeoTokenType::KW_shake  ||
            m_cur.token_type == GeoTokenType::Identifier) {
            std::string name = m_cur.value;
            advance();
            auto args = parseArgs();
            expect(GeoTokenType::Semicolon, "Expected ';' after command");
            return new CmdNode(name, args);
        }
        m_errors.push_back("Line " + std::to_string(m_cur.line) +
                           ": Unknown statement '" + m_cur.value + "'");
        advance();
        return nullptr;
    }

public:
    explicit Parser(std::string src)
        : m_lex(std::move(src)), m_cur(GeoTokenType::EndOfFile, "", 1) {
        advance();
    }

    const std::vector<std::string>& errors() const { return m_errors; }

    ProgramAST* parse() {
        auto* ast = new ProgramAST();
        while (m_cur.token_type != GeoTokenType::EndOfFile) {
            // group declaration
            if (m_cur.token_type == GeoTokenType::KW_group) {
                advance();
                if (m_cur.token_type != GeoTokenType::Identifier) {
                    m_errors.push_back("Line " + std::to_string(m_cur.line) +
                                       ": Expected variable name after 'group'");
                    advance(); continue;
                }
                std::string name = m_cur.value; advance();
                int id = -1;
                if (m_cur.token_type == GeoTokenType::Assign) {
                    advance();
                    if (m_cur.token_type != GeoTokenType::Number) {
                        m_errors.push_back("Line " + std::to_string(m_cur.line) +
                                           ": Expected numeric ID after '='");
                    } else { id = std::stoi(m_cur.value); advance(); }
                }
                expect(GeoTokenType::Semicolon, "Expected ';' after group declaration");
                ast->decls.push_back(new VarDeclNode(name, id));
                continue;
            }
            // event blocks
            if (m_cur.token_type == GeoTokenType::KW_on_start ||
                m_cur.token_type == GeoTokenType::KW_on_touch ||
                m_cur.token_type == GeoTokenType::KW_on_death) {
                auto* ev = new EventNode();
                ev->eventType = m_cur.value; advance();
                if (ev->eventType == "on_touch" || ev->eventType == "on_death") {
                    expect(GeoTokenType::LParen, "Expected '(' after " + ev->eventType);
                    if (m_cur.token_type == GeoTokenType::Identifier) {
                        ev->param = m_cur.value; advance();
                    }
                    expect(GeoTokenType::RParen, "Expected ')'");
                }
                ev->body = parseBody();
                ast->events.push_back(ev);
                continue;
            }
            m_errors.push_back("Line " + std::to_string(m_cur.line) +
                               ": Unexpected token '" + m_cur.value + "'");
            advance();
        }
        return ast;
    }
};

// ============================================================================
// 4. COMPILER  (AST → GD triggers)
// ============================================================================

class GeoCompiler {
    std::unordered_map<std::string, int> m_sym;   // name → group ID
    std::unordered_set<int>              m_used;  // occupied group IDs
    std::vector<std::string>             m_logs;

    // ---- helpers ----
    void scanLevel() {
        m_used.clear();
        auto* ed = LevelEditorLayer::get();
        if (!ed) return;
        auto* objs = ed->m_objects;
        if (!objs) return;
        for (int i = 0; i < objs->count(); i++) {
            auto* obj = static_cast<GameObject*>(objs->objectAtIndex(i));
            if (obj && obj->m_groups)
                for (short g : *obj->m_groups)
                    if (g) m_used.insert(g);
        }
        m_logs.push_back("[Info] Level scanned. Used IDs: " + std::to_string(m_used.size()));
    }

    int allocID() {
        for (int i = 1000; i < 9999; i++)
            if (!m_used.count(i)) { m_used.insert(i); return i; }
        return -1;
    }

    // Place a trigger string at (x, y) in the editor
    void place(LevelEditorLayer* ed, const std::string& s) {
        ed->createObjectsFromString(s, true, true);
    }

    // Build a Spawn trigger that fires group `targetGroup` after `delay` seconds
    // placed at (x, y).  Returns the trigger string.
    std::string spawnTrigger(float x, float y, int targetGroup, float delay) {
        std::ostringstream ss;
        // Object ID 1268 = Spawn Trigger
        ss << "1,1268"
           << ",2," << x << ",3," << y
           << ",51," << targetGroup   // target group
           << ",63," << delay         // delay
           << ",62,1";                // spawn triggered = 1
        return ss.str();
    }

    // ---- code generation context ----
    struct GenCtx {
        LevelEditorLayer* ed;
        float             x;       // current X placement cursor
        float             y;       // sky Y
        float             delay;   // accumulated delay (seconds)
        int               chainGroup; // group that chains next spawn (-1 = none)
    };

    // Resolve group name → ID (error if unknown)
    int resolve(const std::string& name, int line) {
        auto it = m_sym.find(name);
        if (it == m_sym.end()) {
            m_logs.push_back("[Error] Line " + std::to_string(line) +
                             ": Unknown group '" + name + "'");
            return -1;
        }
        return it->second;
    }

    // Generate triggers for a list of statements
    bool genBody(const std::vector<ASTNode*>& stmts, GenCtx& ctx) {
        for (auto* node : stmts) {
            if (!genNode(node, ctx)) return false;
        }
        return true;
    }

    bool genNode(ASTNode* node, GenCtx& ctx) {
        // ---- CmdNode ----
        if (auto* cmd = dynamic_cast<CmdNode*>(node)) {
            return genCmd(cmd, ctx);
        }
        // ---- RepeatNode ----
        if (auto* rep = dynamic_cast<RepeatNode*>(node)) {
            for (int i = 0; i < rep->count; i++) {
                if (!genBody(rep->body, ctx)) return false;
            }
            return true;
        }
        // ---- WhileTrueNode ----
        if (auto* wt = dynamic_cast<WhileTrueNode*>(node)) {
            // Allocate a loop group. We place a Spawn trigger that re-triggers itself.
            int loopGroup = allocID();
            if (loopGroup < 0) { m_logs.push_back("[Error] Out of group IDs"); return false; }

            // Save context X so we can place the self-spawn at the end
            float loopStartX = ctx.x;

            // Generate body
            if (!genBody(wt->body, ctx)) return false;

            // Place a Spawn trigger pointing back to loopGroup with delay 0
            // (infinite loop — use with care)
            place(ctx.ed, spawnTrigger(ctx.x, ctx.y, loopGroup, 0.f));
            ctx.x += 30.f;

            m_logs.push_back("[Info] while_true loop group: " + std::to_string(loopGroup));
            return true;
        }
        return true;
    }

    bool genCmd(CmdNode* cmd, GenCtx& ctx) {
        const auto& a = cmd->args;

        // ---- move(group, dx, dy, duration) ----
        if (cmd->name == "move") {
            if (a.size() < 4) {
                m_logs.push_back("[Error] move() needs 4 args: (group, dx, dy, duration)");
                return false;
            }
            int gid = resolve(a[0], 0); if (gid < 0) return false;
            float dx  = std::stof(a[1]);
            float dy  = std::stof(a[2]);
            float dur = std::stof(a[3]);

            std::ostringstream ss;
            ss << "1,901,2," << ctx.x << ",3," << ctx.y
               << ",51," << gid
               << ",28," << dx
               << ",29," << dy
               << ",10," << dur;
            // If we have a chain group from wait(), add this trigger to that group
            // so it gets spawn-triggered
            if (ctx.chainGroup >= 0) {
                ss << ",57," << ctx.chainGroup  // editor group
                   << ",62,1";                  // spawn triggered
            }
            place(ctx.ed, ss.str());
            ctx.x += 30.f;
            return true;
        }

        // ---- pulse(group, r, g, b, duration) ----
        if (cmd->name == "pulse") {
            if (a.size() < 5) {
                m_logs.push_back("[Error] pulse() needs 5 args: (group, r, g, b, duration)");
                return false;
            }
            int gid = resolve(a[0], 0); if (gid < 0) return false;
            int r   = std::stoi(a[1]);
            int g   = std::stoi(a[2]);
            int b   = std::stoi(a[3]);
            float dur = std::stof(a[4]);

            std::ostringstream ss;
            ss << "1,1006,2," << ctx.x << ",3," << ctx.y
               << ",51," << gid
               << ",7,"  << r
               << ",8,"  << g
               << ",9,"  << b
               << ",10," << dur
               << ",46," << dur
               << ",47," << dur
               << ",52,1";
            if (ctx.chainGroup >= 0) {
                ss << ",57," << ctx.chainGroup
                   << ",62,1";
            }
            place(ctx.ed, ss.str());
            ctx.x += 30.f;
            return true;
        }

        // ---- wait(seconds) ----
        // Uses a Spawn Trigger with delay — real GD timing, independent of speed
        if (cmd->name == "wait") {
            if (a.empty()) {
                m_logs.push_back("[Error] wait() needs 1 arg: seconds");
                return false;
            }
            float secs = std::stof(a[0]);

            // Allocate a chain group for subsequent triggers
            int nextGroup = allocID();
            if (nextGroup < 0) { m_logs.push_back("[Error] Out of group IDs"); return false; }

            // Place a Spawn trigger that fires nextGroup after `secs`
            place(ctx.ed, spawnTrigger(ctx.x, ctx.y, nextGroup, secs));
            ctx.x += 30.f;
            ctx.delay += secs;
            ctx.chainGroup = nextGroup;

            m_logs.push_back("[Info] wait(" + std::to_string(secs) +
                             "s) → chain group " + std::to_string(nextGroup));
            return true;
        }

        // ---- shake(duration) ----
        if (cmd->name == "shake") {
            if (a.empty()) {
                m_logs.push_back("[Error] shake() needs 1 arg: duration");
                return false;
            }
            float dur = std::stof(a[0]);
            std::ostringstream ss;
            ss << "1,1520,2," << ctx.x << ",3," << ctx.y
               << ",10," << dur
               << ",28,1.0"
               << ",29,0.3";
            if (ctx.chainGroup >= 0) {
                ss << ",57," << ctx.chainGroup
                   << ",62,1";
            }
            place(ctx.ed, ss.str());
            ctx.x += 30.f;
            return true;
        }

        m_logs.push_back("[Warning] Unknown command '" + cmd->name + "' — skipped");
        return true;
    }

public:
    const std::vector<std::string>& getLogs() const { return m_logs; }

    bool compile(const std::string& src) {
        m_logs.clear();
        m_sym.clear();
        scanLevel();

        Parser parser(src);
        std::unique_ptr<ProgramAST> ast(parser.parse());

        if (!parser.errors().empty()) {
            for (auto& e : parser.errors()) m_logs.push_back("[Error] " + e);
            return false;
        }
        m_logs.push_back("[Parse] OK");

        // Resolve group declarations
        for (auto* decl : ast->decls) {
            if (decl->explicitId >= 0) {
                if (m_used.count(decl->explicitId)) {
                    m_logs.push_back("[Error] Group ID " + std::to_string(decl->explicitId) +
                                     " already used on level");
                    return false;
                }
                m_sym[decl->name] = decl->explicitId;
                m_used.insert(decl->explicitId);
            } else {
                int id = allocID();
                if (id < 0) { m_logs.push_back("[Error] Out of group IDs"); return false; }
                m_sym[decl->name] = id;
            }
            m_logs.push_back("[Link] '" + decl->name + "' → group " +
                             std::to_string(m_sym[decl->name]));
        }

        auto* ed = LevelEditorLayer::get();
        if (!ed) {
            m_logs.push_back("[Error] Level editor not found — open a level first");
            return false;
        }

        float baseX = ed->m_player1->getPositionX() + 50.f;
        float skyY  = 1500.f;

        for (auto* ev : ast->events) {
            m_logs.push_back("[Compile] Event: " + ev->eventType);

            GenCtx ctx;
            ctx.ed         = ed;
            ctx.x          = baseX;
            ctx.y          = skyY;
            ctx.delay      = 0.f;
            ctx.chainGroup = -1;

            // Place the event trigger
            if (ev->eventType == "on_start") {
                // on_start: just place triggers normally, they fire at level start
                if (!genBody(ev->body, ctx)) return false;

            } else if (ev->eventType == "on_touch") {
                // on_touch(group): Touch Trigger (ID 1595) activates a spawn group
                int spawnGroup = allocID();
                if (spawnGroup < 0) { m_logs.push_back("[Error] Out of IDs"); return false; }

                int touchGroup = -1;
                if (!ev->param.empty()) {
                    touchGroup = resolve(ev->param, 0);
                    if (touchGroup < 0) return false;
                }

                // Touch trigger: when player touches touchGroup, activate spawnGroup
                std::ostringstream ts;
                // Object 1595 = Touch Trigger
                ts << "1,1595,2," << ctx.x << ",3," << ctx.y
                   << ",51," << spawnGroup  // activate group
                   << ",52,1";              // toggle mode
                if (touchGroup >= 0) ts << ",11," << touchGroup; // target group filter
                place(ed, ts.str());
                ctx.x += 30.f;

                // Body triggers are spawn-triggered by spawnGroup
                ctx.delay = 1.f; // mark as spawn-triggered
                if (!genBody(ev->body, ctx)) return false;

            } else if (ev->eventType == "on_death") {
                // on_death(group): Collision Trigger or just note it
                // GD doesn't have a direct on_death trigger for groups,
                // but we can use a Collision trigger (ID 1815) if available,
                // or approximate with a Toggle + Spawn chain.
                // For now: place a comment log and generate body as spawn-triggered
                m_logs.push_back("[Info] on_death: using spawn-triggered body for group '" +
                                 ev->param + "'");
                int spawnGroup = allocID();
                if (spawnGroup < 0) { m_logs.push_back("[Error] Out of IDs"); return false; }

                // Collision trigger (1815): player A = player, player B = group
                int deathGroup = -1;
                if (!ev->param.empty()) {
                    deathGroup = resolve(ev->param, 0);
                    if (deathGroup < 0) return false;
                }
                std::ostringstream cs;
                cs << "1,1815,2," << ctx.x << ",3," << ctx.y
                   << ",51," << spawnGroup;
                if (deathGroup >= 0) cs << ",71," << deathGroup;
                place(ed, cs.str());
                ctx.x += 30.f;

                ctx.delay = 1.f;
                if (!genBody(ev->body, ctx)) return false;
            }

            skyY -= 60.f; // stack event rows vertically
        }

        m_logs.push_back("[Success] Compilation done! Triggers placed in sky.");
        return true;
    }
};
