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
// 1. ЛЕКСИЧЕСКИЙ АНАЛИЗАТОР (LEXER)
// ============================================================================

enum class GeoTokenType {
    KeywordGroup,    // group
    KeywordOnStart,  // on_start
    KeywordOnTouch,  // on_touch
    KeywordMove,     // move
    KeywordPulse,    // pulse
    KeywordWait,     // wait
    Identifier,      // spikes, door, etc.
    Number,          // 10, 0.5, -100
    Assign,          // =
    Semicolon,       // ;
    Comma,           // ,
    LParenthesis,    // (
    RParenthesis,    // )
    LBrace,          // {
    RBrace,          // }
    EndOfFile,
    Error
};

struct GeoToken {
    GeoTokenType token_type;
    std::string value;
    int line;

    // Явный конструктор для устранения ошибок инициализации на MSVC (C2440)
    GeoToken(GeoTokenType t, std::string val, int l) : token_type(t), value(val), line(l) {}
};

class Lexer {
private:
    std::string m_source;
    size_t m_cursor = 0;
    int m_line = 1;

    char peek() const {
        if (m_cursor >= m_source.size()) return '\0';
        return m_source[m_cursor];
    }

    char next() {
        if (m_cursor >= m_source.size()) return '\0';
        char c = m_source[m_cursor++];
        if (c == '\n') m_line++;
        return c;
    }

    void skipWhitespaceAndComments() {
        while (m_cursor < m_source.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                next();
            } else if (c == '/' && m_cursor + 1 < m_source.size() && m_source[m_cursor + 1] == '/') {
                // Однострочный комментарий
                while (peek() != '\n' && peek() != '\0') next();
            } else {
                break;
            }
        }
    }

public:
    Lexer(std::string source) : m_source(source) {}

    GeoToken nextToken() {
        skipWhitespaceAndComments();

        if (m_cursor >= m_source.size()) {
            return GeoToken(GeoTokenType::EndOfFile, "", m_line);
        }

        char c = peek();

        // Парсинг чисел (целых и с плавающей точкой)
        if (std::isdigit(c) || c == '-') {
            std::string numStr;
            // Проверка на отрицательное число или просто минус
            if (c == '-') {
                if (m_cursor + 1 < m_source.size() && std::isdigit(m_source[m_cursor + 1])) {
                    numStr += next();
                } else {
                    next();
                    return GeoToken(GeoTokenType::Error, "-", m_line);
                }
            }
            while (std::isdigit(peek()) || peek() == '.') {
                numStr += next();
            }
            return GeoToken(GeoTokenType::Number, numStr, m_line);
        }

        // Парсинг идентификаторов и ключевых слов
        if (std::isalpha(c) || c == '_') {
            std::string ident;
            while (std::isalnum(peek()) || peek() == '_') {
                ident += next();
            }

            if (ident == "group")    return GeoToken(GeoTokenType::KeywordGroup, ident, m_line);
            if (ident == "on_start") return GeoToken(GeoTokenType::KeywordOnStart, ident, m_line);
            if (ident == "on_touch") return GeoToken(GeoTokenType::KeywordOnTouch, ident, m_line);
            if (ident == "move")     return GeoToken(GeoTokenType::KeywordMove, ident, m_line);
            if (ident == "pulse")    return GeoToken(GeoTokenType::KeywordPulse, ident, m_line);
            if (ident == "wait")     return GeoToken(GeoTokenType::KeywordWait, ident, m_line);

            return GeoToken(GeoTokenType::Identifier, ident, m_line);
        }

        // Одиночные символы
        next();
        switch (c) {
            case '=': return GeoToken(GeoTokenType::Assign, "=", m_line);
            case ';': return GeoToken(GeoTokenType::Semicolon, ";", m_line);
            case ',': return GeoToken(GeoTokenType::Comma, ",", m_line);
            case '(': return GeoToken(GeoTokenType::LParenthesis, "(", m_line);
            case ')': return GeoToken(GeoTokenType::RParenthesis, ")", m_line);
            case '{': return GeoToken(GeoTokenType::LBrace, "{", m_line);
            case '}': return GeoToken(GeoTokenType::RBrace, "}", m_line);
        }

        return GeoToken(GeoTokenType::Error, std::string(1, c), m_line);
    }
};

// ============================================================================
// 2. АБСТРАКТНОЕ СИНТАКСИЧЕСКОЕ ДЕРЕВО (AST)
// ============================================================================

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct VarDeclNode : public ASTNode {
    std::string name;
    int explicitId = -1; // -1 означает автоматический поиск свободного ID

    VarDeclNode(std::string n, int id) : name(n), explicitId(id) {}
};

struct CommandNode : public ASTNode {
    std::string commandName;
    std::vector<std::string> args;

    CommandNode(std::string name, std::vector<std::string> a) : commandName(name), args(a) {}
};

struct EventBlockNode : public ASTNode {
    std::string eventType; // "on_start" или "on_touch"
    std::string triggerParam; // Параметр для on_touch
    std::vector<CommandNode*> commands;

    ~EventBlockNode() {
        for (auto cmd : commands) delete cmd;
    }
};

struct ProgramAST {
    std::vector<VarDeclNode*> declarations;
    std::vector<EventBlockNode*> events;

    ~ProgramAST() {
        for (auto decl : declarations) delete decl;
        for (auto ev : events) delete ev;
    }
};

// ============================================================================
// 3. СИНТАКСИЧЕСКИЙ АНАЛИЗАТОР (PARSER)
// ============================================================================

class Parser {
private:
    Lexer m_lexer;
    GeoToken m_currentToken;
    std::vector<std::string> m_errors;

    void consume() {
        m_currentToken = m_lexer.nextToken();
    }

    void expect(GeoTokenType type, std::string errMsg) {
        if (m_currentToken.token_type != type) {
            m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": " + errMsg);
        } else {
            consume();
        }
    }

public:
    Parser(std::string source) : m_lexer(source), m_currentToken(GeoTokenType::EndOfFile, "", 1) {
        consume();
    }

    std::vector<std::string> getErrors() const { return m_errors; }

    ProgramAST* parse() {
        auto ast = new ProgramAST();

        while (m_currentToken.token_type != GeoTokenType::EndOfFile) {
            if (m_currentToken.token_type == GeoTokenType::KeywordGroup) {
                // Парсим: group name = ID; или group name;
                consume();
                if (m_currentToken.token_type != GeoTokenType::Identifier) {
                    m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Ожидалось имя переменной после 'group'");
                    consume();
                    continue;
                }
                std::string varName = m_currentToken.value;
                consume();

                int explicitId = -1;
                if (m_currentToken.token_type == GeoTokenType::Assign) {
                    consume();
                    if (m_currentToken.token_type != GeoTokenType::Number) {
                        m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Ожидался числовой ID после '='");
                    } else {
                        explicitId = std::stoi(m_currentToken.value);
                        consume();
                    }
                }

                expect(GeoTokenType::Semicolon, "Ожидалась ';' в конце объявления группы");
                ast->declarations.push_back(new VarDeclNode(varName, explicitId));

            } else if (m_currentToken.token_type == GeoTokenType::KeywordOnStart) {
                consume();
                auto evNode = new EventBlockNode();
                evNode->eventType = "on_start";

                expect(GeoTokenType::LBrace, "Ожидалась '{' перед блоком on_start");
                parseBlock(evNode);
                ast->events.push_back(evNode);

            } else if (m_currentToken.token_type == GeoTokenType::KeywordOnTouch) {
                consume();
                auto evNode = new EventBlockNode();
                evNode->eventType = "on_touch";

                expect(GeoTokenType::LParenthesis, "Ожидалась '(' после on_touch");
                if (m_currentToken.token_type != GeoTokenType::Identifier) {
                    m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Ожидалось имя группы в on_touch");
                } else {
                    evNode->triggerParam = m_currentToken.value;
                    consume();
                }
                expect(GeoTokenType::RParenthesis, "Ожидалась ')' после параметра on_touch");
                expect(GeoTokenType::LBrace, "Ожидалась '{' перед блоком on_touch");
                parseBlock(evNode);
                ast->events.push_back(evNode);

            } else {
                m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Неизвестный токен '" + m_currentToken.value + "'");
                consume();
            }
        }
        return ast;
    }

private:
    void parseBlock(EventBlockNode* evNode) {
        while (m_currentToken.token_type != GeoTokenType::RBrace && m_currentToken.token_type != GeoTokenType::EndOfFile) {
            if (m_currentToken.token_type == GeoTokenType::KeywordMove ||
                m_currentToken.token_type == GeoTokenType::KeywordPulse ||
                m_currentToken.token_type == GeoTokenType::KeywordWait) {

                std::string cmdName = m_currentToken.value;
                consume();

                expect(GeoTokenType::LParenthesis, "Ожидалась '(' после имени команды");
                std::vector<std::string> args;
                while (m_currentToken.token_type != GeoTokenType::RParenthesis && m_currentToken.token_type != GeoTokenType::EndOfFile) {
                    if (m_currentToken.token_type == GeoTokenType::Identifier || m_currentToken.token_type == GeoTokenType::Number) {
                        args.push_back(m_currentToken.value);
                        consume();
                    } else {
                        m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Невалидный аргумент '" + m_currentToken.value + "'");
                        consume();
                    }

                    if (m_currentToken.token_type == GeoTokenType::Comma) {
                        consume();
                    } else if (m_currentToken.token_type != GeoTokenType::RParenthesis) {
                        m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Ожидалась ',' или ')'");
                        consume();
                    }
                }
                consume(); // убираем ')'
                expect(GeoTokenType::Semicolon, "Ожидалась ';' после вызова команды");

                evNode->commands.push_back(new CommandNode(cmdName, args));
            } else {
                m_errors.push_back("Строка " + std::to_string(m_currentToken.line) + ": Команды должны быть внутри блоков логики");
                consume();
            }
        }
        consume(); // убираем '}'
    }
};

// ============================================================================
// 4. ГЕНЕРАТОР ТРИГГЕРОВ (COMPILER)
// ============================================================================

class GeoCompiler {
private:
    std::unordered_map<std::string, int> m_symbolTable;
    std::unordered_set<int> m_usedIDs;
    std::vector<std::string> m_logs;

    void scanLevelForGroups() {
        m_usedIDs.clear();
        auto editor = LevelEditorLayer::get();
        if (!editor) return;

        auto objects = editor->m_objects;
        if (objects) {
            for (int i = 0; i < objects->count(); ++i) {
                auto obj = static_cast<GameObject*>(objects->objectAtIndex(i));
                // m_groups объявлен в Geode как std::array<short, 10>*, поэтому разыменовываем его
                if (obj && obj->m_groups) {
                    for (short grp : *obj->m_groups) {
                        if (grp != 0) {
                            m_usedIDs.insert(static_cast<int>(grp));
                        }
                    }
                }
            }
        }
        m_logs.push_back("[Инфо]: Сканирование уровня завершено. Найдено занятых ID: " + std::to_string(m_usedIDs.size()));
    }

    int getNextFreeID() {
        for (int i = 1000; i < 9999; i++) {
            if (m_usedIDs.find(i) == m_usedIDs.end()) {
                m_usedIDs.insert(i);
                return i;
            }
        }
        return -1;
    }

public:
    std::vector<std::string> getLogs() const { return m_logs; }

    bool compile(std::string sourceCode) {
        m_logs.clear();
        scanLevelForGroups();

        Parser parser(sourceCode);
        std::unique_ptr<ProgramAST> ast(parser.parse());

        auto errors = parser.getErrors();
        if (!errors.empty()) {
            for (const auto& err : errors) m_logs.push_back("[Ошибка]: " + err);
            return false;
        }

        m_logs.push_back("[Парсинг]: Успешно завершен без ошибок.");

        // 1. Проход по объявлениям переменных (разрешение ID)
        for (auto decl : ast->declarations) {
            if (decl->explicitId != -1) {
                // Защита от дурака: проверяем, не конфликтует ли назначенный ID
                if (m_usedIDs.find(decl->explicitId) != m_usedIDs.end()) {
                    m_logs.push_back("[Ошибка]: Конфликт! Группа ID " + std::to_string(decl->explicitId) +
                                     " уже физически занята на уровне. Выберите другой ID.");
                    return false;
                }
                m_symbolTable[decl->name] = decl->explicitId;
                m_usedIDs.insert(decl->explicitId);
                m_logs.push_back("[Связь]: Переменная '" + decl->name + "' завязана на ручной ID: " + std::to_string(decl->explicitId));
            } else {
                int autoId = getNextFreeID();
                if (autoId == -1) {
                    m_logs.push_back("[Ошибка]: Закончились свободные группы ID в диапазоне 1000-9999!");
                    return false;
                }
                m_symbolTable[decl->name] = autoId;
                m_logs.push_back("[Связь]: Переменная '" + decl->name + "' автоматически получила ID: " + std::to_string(autoId));
            }
        }

        // 2. Генерация триггеров в Geometry Dash
        auto editor = LevelEditorLayer::get();
        if (!editor) {
            m_logs.push_back("[Ошибка]: Редактор не найден. Откройте уровень перед компиляцией!");
            return false;
        }

        float currentX = editor->m_player1->getPositionX();
        float startY = 1500.0f; // Зона логики строится высоко в небе, чтобы не мешать декору

        for (auto ev : ast->events) {
            m_logs.push_back("[Компилятор]: Обработка блока события '" + ev->eventType + "'");

            if (ev->eventType == "on_start") {
                // Создаем последовательность триггеров в небе
                float triggerX = currentX + 50.f;
                float delayAccumulator = 0.0f;

                for (auto cmd : ev->commands) {
                    if (cmd->commandName == "move") {
                        if (cmd->args.size() < 4) {
                            m_logs.push_back("[Ошибка]: move() требует 4 аргумента: (группа, x, y, время)");
                            return false;
                        }
                        std::string targetVar = cmd->args[0];
                        if (m_symbolTable.find(targetVar) == m_symbolTable.end()) {
                            m_logs.push_back("[Ошибка]: Неизвестная группа '" + targetVar + "'");
                            return false;
                        }
                        int targetID = m_symbolTable[targetVar];
                        float moveX = std::stof(cmd->args[1]);
                        float moveY = std::stof(cmd->args[2]);
                        float duration = std::stof(cmd->args[3]);

                        // Генерация Move-триггера через строку сохранения GD
                        // ID Move-триггера = 901
                        std::stringstream ss;
                        ss << "1,901,2," << triggerX << ",3," << startY
                           << ",51," << targetID  // Target Group
                           << ",28," << moveX     // Move X
                           << ",29," << moveY     // Move Y
                           << ",10," << duration; // Move Time

                        if (delayAccumulator > 0.0f) {
                            // Если была задержка wait(), генерируем Spawn триггер
                            ss << ",104,1"; // Spawn Triggered flag
                        }

                        // В Geode 2.2081 метод принимает 3 аргумента (строка, выделить_объекты, отмена)
                        editor->createObjectsFromString(ss.str(), true, true);
                        triggerX += 30.0f;

                    } else if (cmd->commandName == "wait") {
                        if (cmd->args.empty()) {
                            m_logs.push_back("[Ошибка]: wait() требует 1 аргумент: время задержки");
                            return false;
                        }
                        float waitTime = std::stof(cmd->args[0]);
                        delayAccumulator += waitTime;
                        // Задержка имитируется смещением следующих триггеров по оси X
                        triggerX += (waitTime * 311.8f); // 311.8 - скорость движения на 1x
                    }
                }
            }
        }

        m_logs.push_back("[Успех]: Компиляция завершена! Триггеры размещены в небе.");
        return true;
    }
};
