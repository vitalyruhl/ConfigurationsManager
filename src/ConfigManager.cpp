#include "ConfigManager.h"

// Global instances
AsyncWebServer server(80);
ConfigManagerClass ConfigManager;

namespace {
String resolveLayoutLabel(const char *provided, const String &fallback)
{
    if (provided && provided[0] != '\0')
    {
        return String(provided);
    }
    if (!fallback.isEmpty())
    {
        return fallback;
    }
    return String(ConfigManagerClass::DEFAULT_LAYOUT_NAME);
}
}

// Static logger instances
std::function<void(const char*)> ConfigManagerClass::logger = nullptr;
std::function<void(const char*)> ConfigManagerClass::loggerVerbose = nullptr;

#if CM_ENABLE_LOGGING
void ConfigManagerClass::setLogger(LogCallback cb) {
    logger = cb;
    if (!loggerVerbose) {
        loggerVerbose = cb;
    }
}

void ConfigManagerClass::setLoggerVerbose(LogCallback cb) {
    loggerVerbose = cb;
}

void ConfigManagerClass::log_message(const char *format, ...)
{
    if (!logger && !loggerVerbose) {
        return;
    }
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    const char* msg = buffer;
    bool useVerbose = false;
    if (msg[0] == '[') {
        const char* end = strchr(msg, ']');
        if (end && end > msg + 1) {
            const size_t tokenLen = static_cast<size_t>(end - msg - 1);
            auto tokenEquals = [&](const char* value) {
                const size_t valueLen = strlen(value);
                return valueLen == tokenLen && strncmp(msg + 1, value, tokenLen) == 0;
            };

            const bool isVerboseToken = tokenEquals("DEBUG") || tokenEquals("D") || tokenEquals("TRACE") || tokenEquals("T") ||
                                        tokenEquals("VERBOSE") || tokenEquals("V");
            const bool isLevelToken = isVerboseToken || tokenEquals("INFO") || tokenEquals("I") || tokenEquals("WARN") || tokenEquals("W") ||
                                      tokenEquals("ERROR") || tokenEquals("E") || tokenEquals("FATAL") || tokenEquals("F");

            if (isVerboseToken) {
                useVerbose = true;
            }
            if (isLevelToken) {
                msg = end + 1;
                if (*msg == ' ') {
                    msg++;
                }
            }
        }
    }

    if (useVerbose && loggerVerbose) {
        loggerVerbose(msg);
    } else if (logger) {
        logger(msg);
    } else if (loggerVerbose) {
        loggerVerbose(msg);
    }
}

void ConfigManagerClass::log_verbose_message(const char *format, ...)
{
    if (loggerVerbose) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        loggerVerbose(buffer);
    }
}
#endif

String ConfigManagerClass::normalizeLayoutName(const String &value) const
{
    String normalized = value;
    normalized.trim();
    normalized.toLowerCase();
    return normalized;
}

void ConfigManagerClass::logLayoutWarningOnce(const String &key, const String &message)
{
    if (layoutWarnings.insert(key).second)
    {
        CM_CORE_LOG("[WARNING] %s", message.c_str());
    }
}

ConfigManagerClass::LayoutPage *ConfigManagerClass::findLayoutPage(std::vector<LayoutPage> &pages, const String &normalized)
{
    for (auto &page : pages)
    {
        if (normalizeLayoutName(page.name) == normalized)
        {
            return &page;
        }
    }
    return nullptr;
}

ConfigManagerClass::LayoutCard *ConfigManagerClass::findLayoutCard(LayoutPage &page, const String &normalized)
{
    for (auto &card : page.cards)
    {
        if (normalizeLayoutName(card.name) == normalized)
        {
            return &card;
        }
    }
    return nullptr;
}

ConfigManagerClass::LayoutGroup *ConfigManagerClass::findLayoutGroup(LayoutCard &card, const String &normalized)
{
    for (auto &group : card.groups)
    {
        if (normalizeLayoutName(group.name) == normalized)
        {
            return &group;
        }
    }
    return nullptr;
}

ConfigManagerClass::LayoutPage &ConfigManagerClass::ensureLayoutPage(std::vector<LayoutPage> &pages, const char *name, int order, bool warnOnCreate)
{
    String resolvedName = resolveLayoutLabel(name, String(DEFAULT_LAYOUT_NAME));
    String normalized = normalizeLayoutName(resolvedName);
    if (auto *existing = findLayoutPage(pages, normalized))
    {
        if (order >= 0)
        {
            existing->order = order;
        }
        return *existing;
    }

    LayoutPage page;
    page.name = resolvedName;
    page.order = (order >= 0) ? order : DEFAULT_LAYOUT_ORDER;
    pages.push_back(page);
    if (warnOnCreate)
    {
        logLayoutWarningOnce("page:" + normalized, "Layout page '" + resolvedName + "' was auto-created");
    }
    return pages.back();
}

ConfigManagerClass::LayoutCard &ConfigManagerClass::ensureLayoutCard(LayoutPage &page, const char *name, int order, const String &fallbackName, bool warnOnCreate)
{
    String resolvedName = resolveLayoutLabel(name, fallbackName);
    String normalized = normalizeLayoutName(resolvedName);
    if (auto *existing = findLayoutCard(page, normalized))
    {
        if (order >= 0)
        {
            existing->order = order;
        }
        return *existing;
    }

    LayoutCard card;
    card.name = resolvedName;
    card.order = (order >= 0) ? order : DEFAULT_LAYOUT_ORDER;
    page.cards.push_back(card);
    if (warnOnCreate)
    {
        logLayoutWarningOnce("card:" + normalized, "Layout card '" + resolvedName + "' was auto-created in page '" + page.name + "'");
    }
    return page.cards.back();
}

ConfigManagerClass::LayoutGroup &ConfigManagerClass::ensureLayoutGroup(LayoutCard &card, const char *name, int order, const String &fallbackName, bool warnOnCreate)
{
    String resolvedName = resolveLayoutLabel(name, fallbackName);
    String normalized = normalizeLayoutName(resolvedName);
    if (auto *existing = findLayoutGroup(card, normalized))
    {
        if (order >= 0)
        {
            existing->order = order;
        }
        return *existing;
    }

    LayoutGroup group;
    group.name = resolvedName;
    group.order = (order >= 0) ? order : DEFAULT_LAYOUT_ORDER;
    card.groups.push_back(group);
    if (warnOnCreate)
    {
        logLayoutWarningOnce("group:" + normalized, "Layout group '" + resolvedName + "' was auto-created in card '" + card.name + "'");
    }
    return card.groups.back();
}

void ConfigManagerClass::addSettingsPage(const char *pageName, int order)
{
    ensureLayoutPage(settingsPages, pageName, order, false);
}

void ConfigManagerClass::addSettingsCard(const char *pageName, const char *cardName, int order)
{
    String resolvedPageName = resolveLayoutLabel(pageName, String(DEFAULT_LAYOUT_NAME));
    String normalizedPage = normalizeLayoutName(resolvedPageName);
    bool pageExists = findLayoutPage(settingsPages, normalizedPage) != nullptr;
    LayoutPage &page = ensureLayoutPage(settingsPages, pageName, -1, !pageExists);
    ensureLayoutCard(page, cardName, order, page.name, false);
}

void ConfigManagerClass::addSettingsGroup(const char *pageName, const char *cardName, const char *groupName, int order)
{
    String resolvedPageName = resolveLayoutLabel(pageName, String(DEFAULT_LAYOUT_NAME));
    String normalizedPage = normalizeLayoutName(resolvedPageName);
    bool pageExists = findLayoutPage(settingsPages, normalizedPage) != nullptr;
    LayoutPage &page = ensureLayoutPage(settingsPages, pageName, -1, !pageExists);
    LayoutCard &card = ensureLayoutCard(page, cardName, -1, page.name, false);
    ensureLayoutGroup(card, groupName, order, card.name, false);
}

void ConfigManagerClass::addLivePage(const char *pageName, int order)
{
    ensureLayoutPage(livePages, pageName, order, false);
}

void ConfigManagerClass::addLiveCard(const char *pageName, const char *cardName, int order)
{
    String resolvedPageName = resolveLayoutLabel(pageName, String(DEFAULT_LAYOUT_NAME));
    String normalizedPage = normalizeLayoutName(resolvedPageName);
    bool pageExists = findLayoutPage(livePages, normalizedPage) != nullptr;
    LayoutPage &page = ensureLayoutPage(livePages, pageName, -1, !pageExists);
    ensureLayoutCard(page, cardName, order, String(DEFAULT_LIVE_CARD_NAME), false);
}

void ConfigManagerClass::addLiveGroup(const char *pageName, const char *cardName, const char *groupName, int order)
{
    String resolvedPageName = resolveLayoutLabel(pageName, String(DEFAULT_LAYOUT_NAME));
    String normalizedPage = normalizeLayoutName(resolvedPageName);
    bool pageExists = findLayoutPage(livePages, normalizedPage) != nullptr;
    LayoutPage &page = ensureLayoutPage(livePages, pageName, -1, !pageExists);
    LayoutCard &card = ensureLayoutCard(page, cardName, -1, String(DEFAULT_LIVE_CARD_NAME), false);
    ensureLayoutGroup(card, groupName, order, card.name, false);
}

void ConfigManagerClass::setCategoryLayoutOverride(const char *category, const char *page, const char *card, const char *group, int order)
{
    if (!category || category[0] == '\0')
        return;

    CategoryLayoutOverride overrideEntry;
    overrideEntry.page = (page && page[0]) ? String(page) : String();
    overrideEntry.card = (card && card[0]) ? String(card) : String();
    overrideEntry.group = (group && group[0]) ? String(group) : String();
    overrideEntry.order = order;
    categoryLayoutOverrides[String(category)] = overrideEntry;
}

const ConfigManagerClass::CategoryLayoutOverride *ConfigManagerClass::getCategoryLayoutOverride(const char *category) const
{
    if (!category || category[0] == '\0')
    {
        return nullptr;
    }
    auto it = categoryLayoutOverrides.find(String(category));
    if (it == categoryLayoutOverrides.end())
    {
        return nullptr;
    }
    return &it->second;
}

void ConfigManagerClass::registerSettingPlacement(BaseSetting *setting)
{
    if (!setting || !setting->shouldShowInWeb())
    {
        return;
    }

    const char *category = setting->getCategory();
    String pageName = (category && category[0]) ? String(category) : String(DEFAULT_LAYOUT_NAME);
    String cardName = (setting->getCard() && setting->getCard()[0]) ? String(setting->getCard()) : pageName;
    String groupName = (setting->getCard() && setting->getCard()[0]) ? String(setting->getCard()) : cardName;

    if (const auto *overrideEntry = getCategoryLayoutOverride(category))
    {
        if (!overrideEntry->page.isEmpty())
        {
            pageName = overrideEntry->page;
            if (overrideEntry->card.isEmpty())
            {
                cardName = pageName;
            }
        }
        if (!overrideEntry->card.isEmpty())
        {
            cardName = overrideEntry->card;
        }
        if (!overrideEntry->group.isEmpty())
        {
            groupName = overrideEntry->group;
        }
    }

    addSettingsCard(pageName.c_str(), cardName.c_str(), setting->getCardOrder());
    addSettingsGroup(pageName.c_str(), cardName.c_str(), groupName.c_str(), setting->getSortOrder());
    addToSettingsGroup(setting->getKey(), pageName.c_str(), cardName.c_str(), groupName.c_str(), setting->getSortOrder());
}

namespace {
String resolvePlacementName(const char *provided, const char *fallback)
{
    if (provided && provided[0] != '\0')
        return String(provided);
    return fallback ? String(fallback) : String{};
}
}

void ConfigManagerClass::addToSettings(const char *itemId, const char *pageName, int order)
{
    addToSettingsGroup(itemId, pageName, pageName, pageName, order);
}

void ConfigManagerClass::addToSettingsGroup(const char *itemId, const char *pageName, const char *groupName, int order)
{
    addToSettingsGroup(itemId, pageName, pageName, groupName, order);
}

void ConfigManagerClass::addToSettingsGroup(const char *itemId, const char *pageName, const char *cardName, const char *groupName, int order)
{
    String resolvedPage = resolvePlacementName(pageName, DEFAULT_LAYOUT_NAME);
    String resolvedCard = resolvePlacementName(cardName, resolvedPage.c_str());
    String resolvedGroup = resolvePlacementName(groupName, resolvedCard.c_str());
    addSettingsGroup(resolvedPage.c_str(), resolvedCard.c_str(), resolvedGroup.c_str(), order);
    settingsPlacements.push_back({itemId, resolvedPage, resolvedCard, resolvedGroup, order});
}

void ConfigManagerClass::addToLive(const char *itemId, const char *pageName, int order)
{
    addToLiveGroup(itemId, pageName, DEFAULT_LIVE_CARD_NAME, DEFAULT_LIVE_CARD_NAME, order);
}

void ConfigManagerClass::addToLiveGroup(const char *itemId, const char *pageName, const char *groupName, int order)
{
    addToLiveGroup(itemId, pageName, DEFAULT_LIVE_CARD_NAME, groupName, order);
}

void ConfigManagerClass::addToLiveGroup(const char *itemId, const char *pageName, const char *cardName, const char *groupName, int order)
{
    String resolvedPage = resolvePlacementName(pageName, DEFAULT_LAYOUT_NAME);
    String resolvedCard = resolvePlacementName(cardName, DEFAULT_LIVE_CARD_NAME);
    String resolvedGroup = resolvePlacementName(groupName, resolvedCard.c_str());
    addLiveGroup(resolvedPage.c_str(), resolvedCard.c_str(), resolvedGroup.c_str(), order);
    livePlacements.push_back({itemId, resolvedPage, resolvedCard, resolvedGroup, order});
}

String ConfigManagerClass::buildLiveLayoutJSON() const
{
    DynamicJsonDocument doc(16384);
    JsonObject root = doc.to<JsonObject>();
    JsonArray pagesArray = root.createNestedArray("pages");

    const auto normalizeToken = [this](const String &value) -> String {
        return normalizeLayoutName(value);
    };

    auto collectPlacements = [&](const String &pageName, const String &cardName, const String &groupName, JsonArray target) {
        const String pageNorm = normalizeToken(pageName);
        const String cardNorm = normalizeToken(cardName);
        const String groupNorm = normalizeToken(groupName);

        std::vector<const UiPlacement *> matches;
        matches.reserve(livePlacements.size());
        for (const auto &placement : livePlacements)
        {
            if (normalizeToken(placement.page) == pageNorm &&
                normalizeToken(placement.card) == cardNorm &&
                normalizeToken(placement.group) == groupNorm)
            {
                matches.push_back(&placement);
            }
        }

        std::sort(matches.begin(), matches.end(), [](const UiPlacement *a, const UiPlacement *b) {
            if (a->order == b->order)
            {
                return a->id < b->id;
            }
            return a->order < b->order;
        });

        for (const auto *placement : matches)
        {
            target.add(placement->id);
        }
    };

    auto makeSortedPointers = [](const auto &container) {
        using ElementType = typename std::remove_reference<decltype(container)>::type::value_type;
        std::vector<const ElementType *> result;
        result.reserve(container.size());
        for (const auto &entry : container)
        {
            result.push_back(&entry);
        }
        std::sort(result.begin(), result.end(), [](const ElementType *a, const ElementType *b) {
            if (a->order == b->order)
            {
                return a->name < b->name;
            }
            return a->order < b->order;
        });
        return result;
    };

    const auto sortedPages = makeSortedPointers(livePages);
    for (const auto *page : sortedPages)
    {
        JsonObject pageObj = pagesArray.createNestedObject();
        pageObj["name"] = page->name;
        pageObj["title"] = page->name;
        pageObj["order"] = page->order;
        pageObj["key"] = normalizeToken(page->name);

        JsonArray cardsArray = pageObj.createNestedArray("cards");
        const auto sortedCards = makeSortedPointers(page->cards);
        for (const auto *card : sortedCards)
        {
            JsonObject cardObj = cardsArray.createNestedObject();
            cardObj["name"] = card->name;
            cardObj["title"] = card->name;
            cardObj["order"] = card->order;

            JsonArray groupsArray = cardObj.createNestedArray("groups");
            const auto sortedGroups = makeSortedPointers(card->groups);
            for (const auto *group : sortedGroups)
            {
                JsonObject groupObj = groupsArray.createNestedObject();
                groupObj["name"] = group->name;
                groupObj["title"] = group->name;
                groupObj["order"] = group->order;

                JsonArray itemsArray = groupObj.createNestedArray("items");
                collectPlacements(page->name, card->name, group->name, itemsArray);
            }
        }
    }

    JsonObject placementsObj = root.createNestedObject("placements");
    for (const auto &placement : livePlacements)
    {
        JsonObject placementObj = placementsObj.createNestedObject(placement.id);
        placementObj["page"] = placement.page;
        placementObj["card"] = placement.card;
        placementObj["group"] = placement.group;
        placementObj["order"] = placement.order;
    }

    root["defaultPage"] = sortedPages.empty() ? String() : sortedPages.front()->name;

    String output;
    serializeJson(doc, output);
    return output;
}
