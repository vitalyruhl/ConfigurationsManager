#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace cm {
namespace live_layout_detail {
template <typename Container>
auto sortedPointers(const Container& entries) {
  using Entry = typename Container::value_type;
  std::vector<const Entry*> result;
  result.reserve(entries.size());
  for (const auto& entry : entries)
    result.push_back(&entry);
  std::sort(result.begin(), result.end(), [](const Entry* a, const Entry* b) {
    return a->order == b->order ? a->name < b->name : a->order < b->order;
  });
  return result;
}

template <typename Sink, typename Entry>
bool namedEntry(Sink& sink, const Entry& entry) {
  bool first = true;
  return sink.raw("{") && sink.member(first, "name", entry.name) &&
         sink.member(first, "title", entry.name) && sink.member(first, "order", entry.order);
}
} // namespace live_layout_detail

// Inputs must remain stable across the count and write passes. Existing sorting
// and normalization allocations remain; no temporary JSON document is created.
template <typename Sink, typename Pages, typename Normalize>
bool writeLivePageIndexJson(Sink& sink, const Pages& pages, bool scoped, bool hasContent, bool hasUnassigned, Normalize normalize) {
  const auto ordered = live_layout_detail::sortedPointers(pages);
  bool first = true;
  if (!sink.raw("{") || !sink.member(first, "pageScoped", scoped) ||
      !sink.member(first, "hasLiveContent", hasContent) || !sink.raw(",\"pages\":["))
    return false;
  bool firstPage = true;
  if (scoped)
    for (const auto* page : ordered) {
      if (!firstPage && !sink.raw(","))
        return false;
      firstPage = false;
      bool firstMember = true;
      if (!sink.raw("{") || !sink.member(firstMember, "key", normalize(page->name)) ||
          !sink.member(firstMember, "name", page->name) ||
          !sink.member(firstMember, "title", page->name) ||
          !sink.member(firstMember, "order", page->order) || !sink.raw("}"))
        return false;
    }
  if (scoped && hasUnassigned) {
    if (!firstPage && !sink.raw(","))
      return false;
    if (!sink.raw("{\"key\":\"__unassigned__\",\"name\":\"Other\",\"title\":\"Other\",\"order\":2147483647}"))
      return false;
  }
  if (!sink.raw("],\"defaultPage\":"))
    return false;
  if (scoped && !ordered.empty()) {
    if (!sink.string(normalize(ordered.front()->name)))
      return false;
  } else if (!sink.string(scoped && hasUnassigned ? "__unassigned__" : ""))
    return false;
  return sink.raw("}");
}

template <typename Sink, typename Pages, typename Placements, typename StringType, typename Normalize>
bool writeLiveLayoutJson(Sink& sink, const Pages& pages, const Placements& placements, bool enabled, const StringType* pageKey, Normalize normalize) {
  using Placement = typename Placements::value_type;
  const auto collectItems = [&](const auto& pageName, const auto& cardName, const auto& groupName) {
    const auto pageNorm = normalize(pageName);
    const auto cardNorm = normalize(cardName);
    const auto groupNorm = normalize(groupName);
    std::vector<const Placement*> matches;
    matches.reserve(placements.size());
    for (const auto& placement : placements) {
      if (normalize(placement.page) == pageNorm && normalize(placement.card) == cardNorm &&
          normalize(placement.group) == groupNorm)
        matches.push_back(&placement);
    }
    std::sort(matches.begin(), matches.end(), [](const Placement* a, const Placement* b) {
      return a->order == b->order ? a->id < b->id : a->order < b->order;
    });
    bool first = true;
    for (const auto* placement : matches) {
      if (!first && !sink.raw(","))
        return false;
      first = false;
      if (!sink.string(placement->id))
        return false;
    }
    return true;
  };
  const auto ordered = live_layout_detail::sortedPointers(pages);
  if (!sink.raw("{\"pages\":["))
    return false;
  bool firstPage = true;
  if (enabled)
    for (const auto* page : ordered) {
      if (pageKey && normalize(page->name) != *pageKey)
        continue;
      if (!firstPage && !sink.raw(","))
        return false;
      firstPage = false;
      if (!live_layout_detail::namedEntry(sink, *page) || !sink.raw(",\"key\":") ||
          !sink.string(normalize(page->name)) || !sink.raw(",\"cards\":["))
        return false;
      bool firstCard = true;
      for (const auto* card : live_layout_detail::sortedPointers(page->cards)) {
        if (!firstCard && !sink.raw(","))
          return false;
        firstCard = false;
        if (!live_layout_detail::namedEntry(sink, *card) || !sink.raw(",\"items\":[") ||
            !collectItems(page->name, card->name, StringType()) || !sink.raw("],\"groups\":["))
          return false;
        bool firstGroup = true;
        for (const auto* group : live_layout_detail::sortedPointers(card->groups)) {
          if (!firstGroup && !sink.raw(","))
            return false;
          firstGroup = false;
          if (!live_layout_detail::namedEntry(sink, *group) || !sink.raw(",\"items\":[") ||
              !collectItems(page->name, card->name, group->name) || !sink.raw("]}"))
            return false;
        }
        if (!sink.raw("]}"))
          return false;
      }
      if (!sink.raw("]}"))
        return false;
    }
  if (!sink.raw("],\"placements\":{"))
    return false;
  const auto selected = [&](const Placement& placement) {
    if (!pageKey)
      return true;
    if (*pageKey != "__unassigned__")
      return normalize(placement.page) == *pageKey;
    for (const auto& page : pages)
      if (normalize(page.name) == normalize(placement.page))
        return false;
    return true;
  };
  bool firstPlacement = true;
  for (size_t index = 0; index < placements.size(); ++index) {
    if (!selected(placements[index]))
      continue;
    // ArduinoJson keeps the first key position but overwrites its value with
    // the last selected placement. Preserve both without a second JSON tree.
    bool emitted = false;
    for (size_t earlier = 0; earlier < index; ++earlier) {
      if (selected(placements[earlier]) && placements[earlier].id == placements[index].id) {
        emitted = true;
        break;
      }
    }
    if (emitted)
      continue;
    const auto* placement = &placements[index];
    for (size_t later = index + 1; later < placements.size(); ++later) {
      if (selected(placements[later]) && placements[later].id == placement->id)
        placement = &placements[later];
    }
    if (!firstPlacement && !sink.raw(","))
      return false;
    firstPlacement = false;
    bool firstMember = true;
    if (!sink.string(placement->id) || !sink.raw(":{") ||
        !sink.member(firstMember, "page", placement->page) ||
        !sink.member(firstMember, "card", placement->card) ||
        !sink.member(firstMember, "group", placement->group) ||
        !sink.member(firstMember, "order", placement->order) || !sink.raw("}"))
      return false;
  }
  if (!sink.raw("},\"defaultPage\":"))
    return false;
  // A scoped request historically resolves the title even when layout output
  // is disabled. The unscoped response instead has an empty default then.
  const StringType* defaultPage = !enabled || ordered.empty() ? nullptr : &ordered.front()->name;
  if (pageKey) {
    defaultPage = nullptr;
    for (const auto* page : ordered)
      if (normalize(page->name) == *pageKey) {
        defaultPage = &page->name;
        break;
      }
  }
  if (defaultPage ? !sink.string(*defaultPage) : !sink.string(""))
    return false;
  return sink.raw("}");
}
} // namespace cm
