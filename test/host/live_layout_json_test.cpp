#include <ArduinoJson.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#if __has_include("runtime/detail/LiveLayoutJson.h")
#include "runtime/detail/LiveLayoutJson.h"
#include "web/JsonOutput.h"

// [MOCKED!] Host layout records mirror the fields consumed by the production serializer.
struct Group { std::string name; int order; };
struct Card { std::string name; int order; std::vector<Group> groups; };
struct Page { std::string name; int order; std::vector<Card> cards; };
struct Placement { std::string id, page, card, group; int order; };
struct Output {
  std::string text;
  unsigned reserves = 0;
  void remove(unsigned) { text.clear(); }
  bool reserve(unsigned size) { ++reserves; text.reserve(size); return true; }
  bool concat(char c) { text += c; return true; }
  size_t length() const { return text.length(); }
};
static std::string normalize(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
  for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
template<class T> static auto ordered(const std::vector<T>& values) {
  std::vector<const T*> result;
  for (const auto& value : values) result.push_back(&value);
  std::sort(result.begin(), result.end(), [](const T* a, const T* b) {
    return a->order == b->order ? a->name < b->name : a->order < b->order;
  });
  return result;
}
// Reference reproduces the pre-migration ArduinoJson document contract.
static std::string reference(const std::vector<Page>& pages, const std::vector<Placement>& placements,
                             bool enabled, const std::string* key) {
  JsonDocument doc;
  auto outputPages = doc["pages"].to<JsonArray>();
  const auto sorted = ordered(pages);
  auto items = [&](const Page& page, const Card& card, const std::string& group, JsonArray target) {
    std::vector<const Placement*> matches;
    for (const auto& p : placements)
      if (normalize(p.page) == normalize(page.name) && normalize(p.card) == normalize(card.name) && normalize(p.group) == normalize(group)) matches.push_back(&p);
    std::sort(matches.begin(), matches.end(), [](const Placement* a, const Placement* b) {
      return a->order == b->order ? a->id < b->id : a->order < b->order;
    });
    for (const auto* p : matches) target.add(p->id);
  };
  if (enabled) for (const auto* page : sorted) {
    if (key && normalize(page->name) != *key) continue;
    auto p = outputPages.add<JsonObject>();
    p["name"] = page->name; p["title"] = page->name; p["order"] = page->order; p["key"] = normalize(page->name);
    auto cards = p["cards"].to<JsonArray>();
    for (const auto* card : ordered(page->cards)) {
      auto c = cards.add<JsonObject>();
      c["name"] = card->name; c["title"] = card->name; c["order"] = card->order;
      items(*page, *card, {}, c["items"].to<JsonArray>());
      auto groups = c["groups"].to<JsonArray>();
      for (const auto* group : ordered(card->groups)) {
        auto g = groups.add<JsonObject>();
        g["name"] = group->name; g["title"] = group->name; g["order"] = group->order;
        items(*page, *card, group->name, g["items"].to<JsonArray>());
      }
    }
  }
  auto target = doc["placements"].to<JsonObject>();
  for (const auto& p : placements) {
    bool selected = !key || normalize(p.page) == *key;
    if (key && *key == "__unassigned__") {
      selected = true;
      for (const auto& page : pages) if (normalize(page.name) == normalize(p.page)) selected = false;
    }
    if (!selected) continue;
    auto entry = target[p.id].to<JsonObject>();
    entry["page"] = p.page; entry["card"] = p.card; entry["group"] = p.group; entry["order"] = p.order;
  }
  std::string defaultPage = !enabled || sorted.empty() ? std::string() : sorted.front()->name;
  if (key) {
    defaultPage.clear();
    for (const auto* p : sorted) if (normalize(p->name) == *key) { defaultPage = p->name; break; }
  }
  doc["defaultPage"] = defaultPage;
  std::string result; serializeJson(doc, result); return result;
}
static void compareLayout(const std::vector<Page>& pages, const std::vector<Placement>& placements,
                          bool enabled, const std::string* key) {
  Output output;
  assert(cm::buildJsonOutput([&](auto& sink) { return cm::writeLiveLayoutJson(sink, pages, placements, enabled, key, normalize); }, output));
  const auto expected = reference(pages, placements, enabled, key);
  if (output.text != expected) { std::cerr << output.text << "\nEXPECTED\n" << expected << '\n'; std::abort(); }
  assert(output.reserves == 1);
}
static void compareIndex(const std::vector<Page>& pages, bool scoped, bool content, bool unassigned) {
  JsonDocument doc;
  doc["pageScoped"] = scoped; doc["hasLiveContent"] = content;
  auto target = doc["pages"].to<JsonArray>();
  const auto sorted = ordered(pages);
  if (scoped) for (const auto* page : sorted) {
    auto p = target.add<JsonObject>();
    p["key"] = normalize(page->name); p["name"] = page->name; p["title"] = page->name; p["order"] = page->order;
  }
  if (scoped && unassigned) {
    auto p = target.add<JsonObject>();
    p["key"] = "__unassigned__"; p["name"] = "Other"; p["title"] = "Other"; p["order"] = 2147483647;
  }
  doc["defaultPage"] = scoped && !sorted.empty() ? normalize(sorted.front()->name) : (target.size() ? std::string("__unassigned__") : std::string());
  std::string expected; serializeJson(doc, expected);
  Output output;
  assert(cm::buildJsonOutput([&](auto& sink) { return cm::writeLivePageIndexJson(sink, pages, scoped, content, unassigned, normalize); }, output));
  assert(output.text == expected);
  assert(output.reserves == 1);
}
int main() {
  const std::vector<Page> pages = {
    {" Zeta ", 2, {}},
    {"Alpha\"\\\n", 1, {{"B", 5, {}}, {" A ", 0, {{"G\t\"", 2}, {"empty", 1}}}}},
    {" alpha\"\\\n ", 3, {}}
  };
  const std::vector<Placement> placements = {
    {"duplicate\"", "Alpha\"\\\n", "a", "", 5},
    {"first", "alpha\"\\\n", "a", "", 0},
    {"grouped", "alpha\"\\\n", "a", "G\t\"", -1},
    {"duplicate\"", "Zeta", "B", "", 99},
    {"unassigned", "missing", "C", "", 4},
    {"duplicate\"", "alpha\"\\\n", "a", "", 7},
    {"unicode\xc3\xa4", "Zeta", "", "", 2}
  };
  const std::string keys[] = {"alpha\"\\", "zeta", "__unassigned__", "missing"};
  for (bool enabled : {false, true}) {
    compareLayout(pages, placements, enabled, nullptr);
    compareLayout({}, {}, enabled, nullptr);
    for (const auto& key : keys) compareLayout(pages, placements, enabled, &key);
  }
  for (bool scoped : {false, true}) for (bool content : {false, true}) for (bool unassigned : {false, true}) {
    compareIndex(pages, scoped, content, unassigned);
    compareIndex({}, scoped, content, unassigned);
  }
  std::cout << "live layout output host tests passed\n";
}
#else
int main() { std::cerr << "Missing document-free live layout serializer\n"; return 1; }
#endif
