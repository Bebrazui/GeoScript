#include "GeoIDE.hpp"
#include "Compiler.hpp"
#include "Editor/CustomTextEditor.hpp"

using namespace geode::prelude;

// ============================================================================
// ВСПРЕСОГАТЕЛЬНАЯ ФУНКЦИЯ ДЛЯ ПЕРЕВОДА ЛОГОВ (УСТРАНЯЕТ БАГ С ШРИФТАМИ GD)
// ============================================================================
std::string translateToEnglish(std::string str) {
    std::unordered_map<std::string, std::string> translations = {
        {"[Инфо]", "[Info]"},
        {"[Ошибка]", "[Error]"},
        {"[Связь]", "[Link]"},
        {"[Парсинг]", "[Parsing]"},
        {"[Компилятор]", "[Compiler]"},
        {"[Успех]", "[Success]"},
        {"Сканирование уровня завершено. Найдено занятых ID", "Level scanning completed. Found used IDs"},
        {"Строка", "Line"},
        {"Ожидалось имя переменной после", "Expected variable name after"},
        {"Ожидался числовой ID после", "Expected numeric ID after"},
        {"Ожидалась", "Expected"},
        {"в конце объявления группы", "at the end of group declaration"},
        {"перед блоком", "before block"},
        {"после", "after"},
        {"Ожидалось имя группы в", "Expected group name in"},
        {"после параметра", "after parameter"},
        {"Неизвестный токен", "Unknown token"},
        {"Невалидный аргумент", "Invalid argument"},
        {"или", "or"},
        {"Команды должны быть внутри блоков логики", "Commands must be inside logic blocks"},
        {"Успешно завершен без ошибок", "Successfully completed without errors"},
        {"Конфликт! Группа ID", "Conflict! Group ID"},
        {"уже физически занята на уровне. Выберите другой ID", "is already physically used on the level. Choose another ID"},
        {"Переменная", "Variable"},
        {"завязана на ручной ID", "bound to manual ID"},
        {"автоматически получила ID", "automatically got ID"},
        {"Редактор не найден. Откройте уровень перед компиляцией!", "Editor not found. Open the level before compiling!"},
        {"Обработка блока события", "Processing event block"},
        {"требует 4 аргумента: (группа, x, y, время)", "requires 4 arguments: (group, x, y, time)"},
        {"Неизвестная группа", "Unknown group"},
        {"требует 1 аргумент: время задержки", "requires 1 argument: delay time"},
        {"Компиляция завершена! Триггеры размещены в небе", "Compilation completed! Triggers placed in the sky"}
    };

    for (auto const& [ru, en] : translations) {
        size_t pos = 0;
        while ((pos = str.find(ru, pos)) != std::string::npos) {
            str.replace(pos, ru.length(), en);
            pos += en.length();
        }
    }
    return str;
}

// ============================================================================
// КАСТОМНЫЙ РЕДАКТОР КОДА: Изолированная логика ввода
// ============================================================================
class ScrollableEditor : public CCNode {
public:
    CCTextInputNode* m_input;

    static ScrollableEditor* create(CCSize size) {
        auto ret = new ScrollableEditor();
        if (ret && ret->init(size)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(CCSize size) {
        if (!CCNode::init()) return false;
        this->setContentSize(size);

        auto bg = CCDrawNode::create();
        bg->drawPolygon(
            new CCPoint[4]{{0, 0}, {size.width, 0}, {size.width, size.height}, {0, size.height}},
            4, 
            {15 / 255.f, 15 / 255.f, 15 / 255.f, 1.0f},
            1.5f, 
            {40 / 255.f, 40 / 255.f, 40 / 255.f, 1.0f}
        );
        this->addChild(bg);

        m_input = CCTextInputNode::create(size.width - 24.f, size.height - 24.f, "Write GeoScript here...", "chatFont.fnt");
        m_input->setPosition({size.width / 2.f, size.height / 2.f});
        m_input->setMaxLabelLength(5000);
        m_input->setAllowedChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_()={};,.- \n\t");
        
        this->addChild(m_input);
        return true;
    }
};

// ============================================================================
// РЕАЛИЗАЦИЯ КЛАССА IDE
// ============================================================================

GeoIDE* GeoIDE::create() {
    auto ret = new GeoIDE();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool GeoIDE::init() {
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    if (!CCLayerColor::initWithColor({20, 20, 20, 240}, winSize.width, winSize.height))
        return false;

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);
    this->setKeyboardEnabled(true);
    auto keyboard = cocos2d::CCDirector::sharedDirector()->getKeyboardDispatcher();
    keyboard->addDelegate(dynamic_cast<cocos2d::CCKeyboardDelegate*>(this));
    CCTouchDispatcher::get()->addTargetedDelegate(this, -128, true);

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    this->addChild(menu, 10);

    float sidebarWidth = 140.f;
    float topBarHeight = 40.f;
    float consoleHeight = 80.f;

    float editorX = sidebarWidth + 15.f;
    float editorY = consoleHeight + 25.f;
    float editorWidth = winSize.width - editorX - 15.f;
    float editorHeight = winSize.height - editorY - topBarHeight;

    auto sidebarBg = CCDrawNode::create();
    sidebarBg->drawPolygon(
        new CCPoint[4]{{0, 0}, {sidebarWidth, 0}, {sidebarWidth, winSize.height}, {0, winSize.height}},
        4, 
        {28 / 255.f, 28 / 255.f, 28 / 255.f, 1.0f},
        1.f, 
        {38 / 255.f, 38 / 255.f, 38 / 255.f, 1.0f}
    );
    this->addChild(sidebarBg, 1);

    auto sidebarTitle = CCLabelBMFont::create("ACTIONS", "goldFont.fnt");
    sidebarTitle->setScale(0.6f);
    sidebarTitle->setPosition({sidebarWidth / 2.f, winSize.height - 25.f});
    this->addChild(sidebarTitle, 2);

    auto compileSpr = ButtonSprite::create("Compile", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto compileBtn = CCMenuItemSpriteExtra::create(
        compileSpr,
        this,
        menu_selector(GeoIDE::onCompile)
    );
    compileBtn->setPosition({sidebarWidth / 2.f, winSize.height - 75.f});
    menu->addChild(compileBtn);

    auto title = CCLabelBMFont::create("GeoScript Editor", "goldFont.fnt");
    title->setAnchorPoint({0.0f, 0.5f});
    title->setScale(0.7f);
    title->setPosition({editorX, winSize.height - 25.f});
    this->addChild(title, 2);

    m_editor = CustomTextEditor::create({editorWidth, editorHeight});
    m_editor->setPosition({editorX, editorY});
    this->addChild(m_editor, 2);

    auto consoleBg = CCDrawNode::create();
    consoleBg->drawPolygon(
        new CCPoint[4]{{0, 0}, {editorWidth, 0}, {editorWidth, consoleHeight}, {0, consoleHeight}},
        4, 
        {12 / 255.f, 12 / 255.f, 12 / 255.f, 1.0f},
        1.f, 
        {25 / 255.f, 25 / 255.f, 25 / 255.f, 1.0f}
    );
    
    m_consoleNode = CCNode::create();
    m_consoleNode->setContentSize({editorWidth, consoleHeight});
    m_consoleNode->setPosition({editorX, 15.f});
    m_consoleNode->addChild(consoleBg);
    this->addChild(m_consoleNode, 2);

    m_consoleText = CCLabelBMFont::create(
        "Console ready. Write code and press compile...\n", 
        "chatFont.fnt", 
        editorWidth - 24.f, 
        kCCTextAlignmentLeft
    );
    m_consoleText->setScale(0.45f);
    m_consoleText->setAnchorPoint({0.0f, 1.0f});
    m_consoleText->setPosition({12.f, consoleHeight - 10.f});
    m_consoleText->setColor({140, 140, 140});
    m_consoleNode->addChild(m_consoleText);

    auto closeBtn = CCMenuItemSpriteExtra::create(
        this->createCustomCloseButton(),
        this,
        menu_selector(GeoIDE::onCloseBtn)
    );
    closeBtn->setPosition({ winSize.width - 22.f, winSize.height - 22.f });
    menu->addChild(closeBtn);

    return true;
}

void GeoIDE::keyDown(cocos2d::enumKeyCodes key, double) {
    geode::log::info("GeoIDE::keyDown key: {}", (int)key);

    // Временно попробуем передавать вообще всё, что приходит в GeoIDE, в редактор
    m_editor->keyDown(key, 0.0);
}

cocos2d::CCNode* GeoIDE::createCustomCloseButton() {
    auto node = cocos2d::CCNode::create();
    node->setContentSize({30, 30});
    auto draw = cocos2d::CCDrawNode::create();
    draw->drawDot({15, 15}, 12, {180, 50, 50, 255});
    draw->drawSegment({11, 11}, {19, 19}, 1.5f, {255, 255, 255, 255});
    draw->drawSegment({19, 11}, {11, 19}, 1.5f, {255, 255, 255, 255});
    node->addChild(draw);
    return node;
}

void GeoIDE::onCompile(cocos2d::CCObject* sender) {
    std::string code = m_editor->getText();
    
    m_consoleText->setString("Compilation started...\n");
    m_consoleText->setColor({255, 255, 255});

    auto editor = LevelEditorLayer::get();
    if (!editor) {
        m_consoleText->setString("Error: Level editor layer not found!");
        m_consoleText->setColor({255, 120, 120});
        return;
    }

    auto objects = editor->m_objects;
    if (objects) {
        std::vector<GameObject*> toDelete;
        for (int i = 0; i < objects->count(); ++i) {
            auto obj = static_cast<GameObject*>(objects->objectAtIndex(i));
            if (obj && obj->m_groups) {
                for (short grp : *obj->m_groups) {
                    if (grp == 9999) {
                        toDelete.push_back(obj);
                        break;
                    }
                }
            }
        }
        for (auto obj : toDelete) {
            editor->removeObject(obj, true);
        }
    }

    GeoCompiler compiler;
    bool success = compiler.compile(code);

    std::stringstream logStream;
    for (const auto& logLine : compiler.getLogs()) {
        logStream << translateToEnglish(logLine) << "\n";
    }

    m_consoleText->setString(logStream.str().c_str());

    if (success) {
        m_consoleText->setColor({120, 255, 120});
        editor->updateOptions();
    } else {
        m_consoleText->setColor({255, 120, 120});
    }
}

bool GeoIDE::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
    return true;
}

void GeoIDE::onCloseBtn(cocos2d::CCObject*) {
    this->removeFromParentAndCleanup(true);
}