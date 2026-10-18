#pragma once
#include "RuntimeMetaView.h"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <functional>
#include <type_traits>
#include <utility>

namespace cm { namespace detail {
// Scoped checked storage: growth moves only after allocation succeeds.
// No throwing std::vector allocation is used by retained compact metadata.
struct RuntimeHeap {
  static void* allocate(size_t n) { return std::malloc(n); }
  static void deallocate(void* p) { std::free(p); }
};
template<class T, class Heap = RuntimeHeap> class CheckedVector {
  T* data_ = nullptr;
  size_t size_ = 0, capacity_ = 0;
public:
  CheckedVector() = default;
  ~CheckedVector() { clear(); Heap::deallocate(data_); }
  CheckedVector(const CheckedVector&) = delete;
  CheckedVector& operator=(const CheckedVector&) = delete;
  CheckedVector(CheckedVector&& v) noexcept { swap(v); }
  CheckedVector& operator=(CheckedVector&& v) noexcept { swap(v); return *this; }
  void swap(CheckedVector& v) noexcept { std::swap(data_, v.data_); std::swap(size_, v.size_); std::swap(capacity_, v.capacity_); }
  bool reserve(size_t n) {
    if (n <= capacity_) return true;
    if (n > std::numeric_limits<size_t>::max() / sizeof(T)) return false;
    T* next = static_cast<T*>(Heap::allocate(n * sizeof(T)));
    if (!next) return false;
    for (size_t i = 0; i < size_; ++i) { new (next + i) T(std::move(data_[i])); data_[i].~T(); }
    Heap::deallocate(data_); data_ = next; capacity_ = n; return true;
  }
  bool ensureOne() {
    if (size_ < capacity_) return true;
    if (capacity_ > std::numeric_limits<size_t>::max() - capacity_ / 2 - 1) return false;
    return reserve(capacity_ ? capacity_ + capacity_ / 2 + 1 : 4);
  }
  bool push_back(T&& value) { if (!ensureOne()) return false; new(data_ + size_++) T(std::move(value)); return true; }
  template<class... A> bool emplace_back(A&&... args) { if (!ensureOne()) return false; new(data_ + size_++) T(std::forward<A>(args)...); return true; }
  void pop_back() { if (size_) data_[--size_].~T(); }
  void erase(size_t i) {
    data_[i].~T();
    for (size_t j = i + 1; j < size_; ++j) { new(data_ + j - 1) T(std::move(data_[j])); data_[j].~T(); }
    --size_;
  }
  void clear() { while(size_) pop_back(); }
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  bool empty() const { return !size_; }
  T& operator[](size_t i) { return data_[i]; }
  const T& operator[](size_t i) const { return data_[i]; }
  T* begin() { return data_; } T* end() { return size_ ? data_ + size_ : data_; }
  const T* begin() const { return data_; } const T* end() const { return size_ ? data_ + size_ : data_; }
};
inline bool copyRuntimeString(String& to, const String& from) {
  String candidate;
  if (from.length() && (!candidate.reserve(from.length()) || !candidate.concat(from.c_str(), from.length()))) return false;
  to = std::move(candidate); return true;
}
inline bool sameRuntimeString(const String& a, const String& b) {
  return a.length() == b.length() && (!a.length() || !std::memcmp(a.c_str(), b.c_str(), a.length()));
}
// Expose the pinned Arduino String capacity without adding retained bytes.
class OwnedRuntimeString : public String {
public:
  using String::operator=;
  void reset() { this->~OwnedRuntimeString(); new(this) OwnedRuntimeString(); }
  size_t allocatedBytes() const { return isSSO() ? 0 : static_cast<size_t>(capacity()) + 1; }
};
static_assert(sizeof(OwnedRuntimeString) == sizeof(String), "Compact String accounting must add no object bytes.");
struct StoredLabels { OwnedRuntimeString onLabel, offLabel; };
struct StoredRuntimeFieldBase {
  OwnedRuntimeString key, label;
  int order = 100;
  uint32_t group = 0, sourceGroup = 0, page = 0, card = 0;
};
struct FloatPayload { uint32_t unit = 0; int precision = 2; };
struct IntPayload { uint32_t unit = 0; };
struct EmptyPayload {};
struct StatePayload { bool initialState = false; };
struct IntRangePayload { uint32_t unit = 0; int intMin = 0, intMax = 100, intInit = 0; };
struct FloatRangePayload { uint32_t unit = 0; float floatMin = 0, floatMax = 1, floatInit = 0; int precision = 2; };
template<class P> struct StoredRecord { StoredRuntimeFieldBase base; P payload; };
struct StoredProperty { OwnedRuntimeString key, value; };
struct StoredRule { OwnedRuntimeString target; CheckedVector<StoredProperty> properties; bool hasVisible = false, visible = true; OwnedRuntimeString className; };
struct StoredStyle { CheckedVector<StoredRule> rules; bool empty() const { return rules.empty(); } };
inline bool cloneStyle(StoredStyle& to, const RuntimeFieldStyle& from) {
  if (!to.rules.reserve(from.rules.size())) return false;
  for (const auto& r : from.rules) {
    StoredRule next;
    if (!copyRuntimeString(next.target, r.target) || !copyRuntimeString(next.className, r.className) || !next.properties.reserve(r.properties.size())) return false;
    next.hasVisible = r.hasVisible; next.visible = r.visible;
    for (const auto& p : r.properties) {
      StoredProperty prop;
      if (!copyRuntimeString(prop.key, p.key) || !copyRuntimeString(prop.value, p.value)) return false;
      next.properties.push_back(std::move(prop));
    }
    to.rules.push_back(std::move(next));
  }
  return true;
}
template<class T> struct MetaTraits;
#define CM_META_TRAIT(type, kindName, payloadType, poolName) \
 template<> struct MetaTraits<type> { using Payload = payloadType; static constexpr RuntimeFieldKind kind = RuntimeFieldKind::kindName; };
CM_META_TRAIT(RuntimeFloatMeta, Float, FloatPayload, floats)
CM_META_TRAIT(RuntimeIntMeta, Int, IntPayload, ints)
CM_META_TRAIT(RuntimeBoolMeta, Bool, EmptyPayload, bools)
CM_META_TRAIT(RuntimeTextMeta, Text, EmptyPayload, texts)
CM_META_TRAIT(RuntimeDividerMeta, Divider, EmptyPayload, dividers)
CM_META_TRAIT(RuntimeButtonMeta, Button, EmptyPayload, buttons)
CM_META_TRAIT(RuntimeCheckboxMeta, Checkbox, EmptyPayload, checkboxes)
CM_META_TRAIT(RuntimeStateButtonMeta, StateButton, StatePayload, stateButtons)
CM_META_TRAIT(RuntimeMomentaryButtonMeta, MomentaryButton, EmptyPayload, momentaryButtons)
CM_META_TRAIT(RuntimeIntSliderMeta, IntSlider, IntRangePayload, intSliders)
CM_META_TRAIT(RuntimeIntInputMeta, IntInput, IntRangePayload, intInputs)
CM_META_TRAIT(RuntimeFloatSliderMeta, FloatSlider, FloatRangePayload, floatSliders)
CM_META_TRAIT(RuntimeFloatInputMeta, FloatInput, FloatRangePayload, floatInputs)
#undef CM_META_TRAIT
} // namespace detail

class CompactRuntimeMetaStore {
  using Base = detail::StoredRuntimeFieldBase;
  template<class T> using Buffer = detail::CheckedVector<T>;
  template<class P> using Records = Buffer<detail::StoredRecord<P>>;
  struct TextEntry { detail::OwnedRuntimeString value; uint32_t uses = 0; };
  struct Handle { uint32_t index = 0; RuntimeFieldKind kind = RuntimeFieldKind::Invalid; bool promoted = false; };
  template<class T> struct Sparse { RuntimeFieldId id; T value; };
  Buffer<TextEntry> text_;
  Buffer<Handle> handles_;
  Records<detail::FloatPayload> floats_;
  Records<detail::IntPayload> ints_;
  Records<detail::EmptyPayload> bools_, texts_, dividers_, buttons_, checkboxes_, momentaryButtons_;
  Records<detail::StatePayload> stateButtons_;
  Records<detail::IntRangePayload> intSliders_, intInputs_;
  Records<detail::FloatRangePayload> floatSliders_, floatInputs_;
  Buffer<Sparse<RuntimeNumericAlarm>> numericAlarms_;
  Buffer<Sparse<RuntimeBoolAlarm>> boolAlarms_;
  Buffer<Sparse<detail::StoredLabels>> labels_;
  Buffer<Sparse<detail::StoredStyle>> styles_;
  Buffer<Sparse<detail::OwnedRuntimeString>> staticTexts_;

  const Handle* handle(RuntimeFieldId id) const { return id.value && id.value <= handles_.size() ? &handles_[id.value-1] : nullptr; }
  Handle* handle(RuntimeFieldId id) { return id.value && id.value <= handles_.size() ? &handles_[id.value-1] : nullptr; }
  template<class T> auto& pool() {
#define CM_POOL(type, name) if constexpr (std::is_same<T, type>::value) return name##_; else
    CM_POOL(RuntimeFloatMeta, floats) CM_POOL(RuntimeIntMeta, ints) CM_POOL(RuntimeBoolMeta, bools)
    CM_POOL(RuntimeTextMeta, texts) CM_POOL(RuntimeDividerMeta, dividers) CM_POOL(RuntimeButtonMeta, buttons)
    CM_POOL(RuntimeCheckboxMeta, checkboxes) CM_POOL(RuntimeStateButtonMeta, stateButtons) CM_POOL(RuntimeMomentaryButtonMeta, momentaryButtons)
    CM_POOL(RuntimeIntSliderMeta, intSliders) CM_POOL(RuntimeIntInputMeta, intInputs) CM_POOL(RuntimeFloatSliderMeta, floatSliders)
    return floatInputs_;
#undef CM_POOL
  }
  template<class T> const auto& pool() const { return const_cast<CompactRuntimeMetaStore*>(this)->template pool<T>(); }
  const String& text(uint32_t id) const { return id ? text_[id-1].value : RuntimeMetaView::emptyString(); }
  bool acquire(const String& value, uint32_t& id) {
    id = 0; if (!value.length()) return true;
    for (size_t i = 0; i < text_.size(); ++i) if (text_[i].uses && detail::sameRuntimeString(text_[i].value, value)) {
      if (text_[i].uses == UINT32_MAX) return false;
      ++text_[i].uses; id = static_cast<uint32_t>(i+1); return true;
    }
    TextEntry entry;
    if (!detail::copyRuntimeString(entry.value, value)) return false;
    entry.uses = 1;
    for (size_t i = 0; i < text_.size(); ++i) if (!text_[i].uses) { text_[i] = std::move(entry); id = static_cast<uint32_t>(i+1); return true; }
    if (text_.size() >= UINT32_MAX || !text_.push_back(std::move(entry))) return false;
    id = static_cast<uint32_t>(text_.size()); return true;
  }
  void release(uint32_t id) { if (id && !--text_[id-1].uses) text_[id-1].value.reset(); }
  void releaseBase(const Base& b) { release(b.group); release(b.sourceGroup); release(b.page); release(b.card); }
  template<class P> void releaseRecord(const detail::StoredRecord<P>& r) {
    releaseBase(r.base);
    if constexpr (!std::is_same<P, detail::EmptyPayload>::value && !std::is_same<P, detail::StatePayload>::value) release(r.payload.unit);
  }
  struct TextTransaction {
    CompactRuntimeMetaStore& store; uint32_t acquired[5]{}; size_t count = 0; bool committed = false;
    ~TextTransaction() { if (!committed) for(size_t i=0;i<count;++i) store.release(acquired[i]); }
    bool get(const String& s, uint32_t& id) { if (!store.acquire(s,id)) return false; acquired[count++] = id; return true; }
  };
  template<class T> bool prepare(const T& d, detail::StoredRecord<typename detail::MetaTraits<T>::Payload>& r, TextTransaction& tx) {
    if (!detail::copyRuntimeString(r.base.key,d.base.key) || !detail::copyRuntimeString(r.base.label,d.base.label)) return false;
    r.base.order = d.base.order;
    if (!tx.get(d.base.group,r.base.group) || !tx.get(d.base.sourceGroup,r.base.sourceGroup) || !tx.get(d.base.page,r.base.page) || !tx.get(d.base.card,r.base.card)) return false;
    using P = typename detail::MetaTraits<T>::Payload;
    if constexpr (!std::is_same<P,detail::EmptyPayload>::value && !std::is_same<P,detail::StatePayload>::value) if (!tx.get(d.unit,r.payload.unit)) return false;
    if constexpr (std::is_same<P,detail::FloatPayload>::value || std::is_same<P,detail::FloatRangePayload>::value) r.payload.precision=d.precision;
    if constexpr (std::is_same<P,detail::StatePayload>::value) r.payload.initialState=d.initialState;
    if constexpr (std::is_same<P,detail::IntRangePayload>::value) { if(d.intMin>d.intMax || d.intInit<d.intMin || d.intInit>d.intMax) return false; r.payload.intMin=d.intMin; r.payload.intMax=d.intMax; r.payload.intInit=d.intInit; }
    if constexpr (std::is_same<P,detail::FloatRangePayload>::value) { if(!(d.floatMin<=d.floatMax) || !(d.floatInit>=d.floatMin && d.floatInit<=d.floatMax)) return false; r.payload.floatMin=d.floatMin; r.payload.floatMax=d.floatMax; r.payload.floatInit=d.floatInit; }
    return true;
  }
  template<class T> static T* sparse(Buffer<Sparse<T>>& b,RuntimeFieldId id) { for(auto& x:b) if(x.id==id)return &x.value; return nullptr; }
  template<class T> static const T* sparse(const Buffer<Sparse<T>>& b,RuntimeFieldId id) { for(const auto& x:b) if(x.id==id)return &x.value; return nullptr; }
  template<class T> static void removeSparse(Buffer<Sparse<T>>& b,RuntimeFieldId id) { for(size_t i=0;i<b.size();++i)if(b[i].id==id){b.erase(i);return;} }
  template<class T> static bool commitSparse(Buffer<Sparse<T>>& b,RuntimeFieldId id,T&& value) { if(auto* old=sparse(b,id)){*old=std::move(value);return true;} return b.push_back(Sparse<T>{id,std::move(value)}); }
  bool active(RuntimeFieldId id) const { const auto* h=handle(id); return h && !h->promoted; }
  static bool numeric(RuntimeFieldKind k) { return k==RuntimeFieldKind::Float || k==RuntimeFieldKind::Int || k==RuntimeFieldKind::IntSlider || k==RuntimeFieldKind::IntInput || k==RuntimeFieldKind::FloatSlider || k==RuntimeFieldKind::FloatInput; }
  static bool boolean(RuntimeFieldKind k) { return k==RuntimeFieldKind::Bool || k==RuntimeFieldKind::Checkbox || k==RuntimeFieldKind::StateButton || k==RuntimeFieldKind::MomentaryButton; }
  template<class T> RuntimeMetaView typedView(RuntimeFieldId id,const Handle& h) const {
    const auto& r=pool<T>()[h.index]; const auto* labels=sparse(labels_,id); const auto* staticText=sparse(staticTexts_,id);
    using P=typename detail::MetaTraits<T>::Payload;
    const String* unit=&RuntimeMetaView::emptyString();
    if constexpr (!std::is_same<P,detail::EmptyPayload>::value && !std::is_same<P,detail::StatePayload>::value) unit=&text(r.payload.unit);
    RuntimeMetaView v(text(r.base.group),text(r.base.sourceGroup),text(r.base.page),r.base.key,r.base.label,*unit,labels?labels->onLabel:RuntimeMetaView::emptyString(),labels?labels->offLabel:RuntimeMetaView::emptyString(),staticText?*staticText:RuntimeMetaView::emptyString(),text(r.base.card));
    v.order=r.base.order;
    if constexpr (std::is_same<P,detail::FloatPayload>::value || std::is_same<P,detail::FloatRangePayload>::value) v.precision=r.payload.precision;
    if constexpr (std::is_same<P,detail::IntPayload>::value || std::is_same<P,detail::IntRangePayload>::value) v.precision=0;
    if constexpr (std::is_same<P,detail::StatePayload>::value) v.initialState=r.payload.initialState;
    if constexpr (std::is_same<P,detail::IntRangePayload>::value){v.intMin=r.payload.intMin;v.intMax=r.payload.intMax;v.intInit=r.payload.intInit;}
    if constexpr (std::is_same<P,detail::FloatRangePayload>::value){v.floatMin=r.payload.floatMin;v.floatMax=r.payload.floatMax;v.floatInit=r.payload.floatInit;v.floatPrecision=r.payload.precision;}
    v.isBool=h.kind==RuntimeFieldKind::Bool;v.isString=h.kind==RuntimeFieldKind::Text;v.isDivider=h.kind==RuntimeFieldKind::Divider;v.isButton=h.kind==RuntimeFieldKind::Button;
    v.isCheckbox=h.kind==RuntimeFieldKind::Checkbox;v.isStateButton=h.kind==RuntimeFieldKind::StateButton;v.isMomentaryButton=h.kind==RuntimeFieldKind::MomentaryButton;
    v.isIntSlider=h.kind==RuntimeFieldKind::IntSlider;v.isIntInput=h.kind==RuntimeFieldKind::IntInput;v.isFloatSlider=h.kind==RuntimeFieldKind::FloatSlider;v.isFloatInput=h.kind==RuntimeFieldKind::FloatInput;
    if(const auto* a=sparse(numericAlarms_,id)){v.hasAlarm=a->hasAlarm;v.alarmMin=a->alarmMin;v.alarmMax=a->alarmMax;v.warnMin=a->warnMin;v.warnMax=a->warnMax;}
    if(const auto* a=sparse(boolAlarms_,id)){v.hasAlarm=a->hasAlarm;v.alarmWhenTrue=a->alarmWhenTrue;v.boolAlarmValue=a->boolAlarmValue;}
    if(const auto* s=sparse(styles_,id))v.style=RuntimeStyleView::from(*s);
    return v;
  }
#define CM_EACH_KIND(X) \
 X(Float,RuntimeFloatMeta) X(Int,RuntimeIntMeta) X(Bool,RuntimeBoolMeta) X(Text,RuntimeTextMeta) X(Divider,RuntimeDividerMeta) X(Button,RuntimeButtonMeta) X(Checkbox,RuntimeCheckboxMeta) X(StateButton,RuntimeStateButtonMeta) X(MomentaryButton,RuntimeMomentaryButtonMeta) X(IntSlider,RuntimeIntSliderMeta) X(IntInput,RuntimeIntInputMeta) X(FloatSlider,RuntimeFloatSliderMeta) X(FloatInput,RuntimeFloatInputMeta)
public:
  template<class T> RuntimeFieldId add(const T& d) {
    if(handles_.size()>=UINT32_MAX)return {};
    auto& p=pool<T>(); if(p.size()>=UINT32_MAX || !handles_.ensureOne() || !p.ensureOne())return {};
    detail::StoredRecord<typename detail::MetaTraits<T>::Payload> r; TextTransaction tx{*this};
    if(!prepare(d,r,tx))return {};
    Handle h{static_cast<uint32_t>(p.size()),detail::MetaTraits<T>::kind,false};
    p.push_back(std::move(r)); handles_.push_back(std::move(h)); tx.committed=true;
    return RuntimeFieldId{static_cast<uint32_t>(handles_.size())};
  }
  RuntimeFieldKind kind(RuntimeFieldId id)const {const auto*h=handle(id);return h?h->kind:RuntimeFieldKind::Invalid;}
  size_t size()const{return handles_.size();}
  RuntimeFieldId idAt(size_t i)const{return i<handles_.size()?RuntimeFieldId{static_cast<uint32_t>(i+1)}:RuntimeFieldId{};}
  size_t promotedLegacyIndex(RuntimeFieldId id)const{const auto*h=handle(id);return h&&h->promoted?h->index:std::numeric_limits<size_t>::max();}
  RuntimeMetaView view(RuntimeFieldId id)const {
    const auto*h=handle(id);if(!h||h->promoted)return {};
    switch(h->kind){
#define CM_CASE_VIEW(k,t) case RuntimeFieldKind::k:return typedView<t>(id,*h);
      CM_EACH_KIND(CM_CASE_VIEW)
#undef CM_CASE_VIEW
      default:return {};
    }
  }
  template<class T> bool read(RuntimeFieldId id,T& out)const {
    if(!active(id)||kind(id)!=detail::MetaTraits<T>::kind)return false;
    const auto v=view(id); T next;
    if(!detail::copyRuntimeString(next.base.key,v.key)||!detail::copyRuntimeString(next.base.label,v.label)||!detail::copyRuntimeString(next.base.group,v.group)||!detail::copyRuntimeString(next.base.sourceGroup,v.sourceGroup)||!detail::copyRuntimeString(next.base.page,v.page)||!detail::copyRuntimeString(next.base.card,v.card))return false;
    next.base.order=v.order; using P=typename detail::MetaTraits<T>::Payload;
    if constexpr(!std::is_same<P,detail::EmptyPayload>::value&&!std::is_same<P,detail::StatePayload>::value)if(!detail::copyRuntimeString(next.unit,v.unit))return false;
    if constexpr(std::is_same<P,detail::FloatPayload>::value||std::is_same<P,detail::FloatRangePayload>::value)next.precision=v.precision;
    if constexpr(std::is_same<P,detail::StatePayload>::value)next.initialState=v.initialState;
    if constexpr(std::is_same<P,detail::IntRangePayload>::value){next.intMin=v.intMin;next.intMax=v.intMax;next.intInit=v.intInit;}
    if constexpr(std::is_same<P,detail::FloatRangePayload>::value){next.floatMin=v.floatMin;next.floatMax=v.floatMax;next.floatInit=v.floatInit;}
    out=std::move(next);return true;
  }
  template<class T> bool replace(RuntimeFieldId id,const T& d){
    auto*h=handle(id);if(!h||h->promoted||h->kind!=detail::MetaTraits<T>::kind)return false;
    detail::StoredRecord<typename detail::MetaTraits<T>::Payload> r;TextTransaction tx{*this};if(!prepare(d,r,tx))return false;
    auto& old=pool<T>()[h->index];releaseRecord(old);old=std::move(r);tx.committed=true;return true;
  }
  template<class T> bool update(RuntimeFieldId id,const std::function<void(T&)>& cb){T d;if(!read(id,d))return false;cb(d);return replace(id,d);}
  bool setNumericAlarm(RuntimeFieldId id,const RuntimeNumericAlarm&a){if(!active(id)||!numeric(kind(id)))return false;return commitSparse(numericAlarms_,id,RuntimeNumericAlarm(a));}
  bool setBoolAlarm(RuntimeFieldId id,const RuntimeBoolAlarm&a){if(!active(id)||!boolean(kind(id)))return false;return commitSparse(boolAlarms_,id,RuntimeBoolAlarm(a));}
  bool setStateLabels(RuntimeFieldId id,const RuntimeStateLabels& a){if(!active(id)||!boolean(kind(id)))return false;detail::StoredLabels next;if(!detail::copyRuntimeString(next.onLabel,a.onLabel)||!detail::copyRuntimeString(next.offLabel,a.offLabel))return false;if(!a.onLabel.length()&&!a.offLabel.length()){removeSparse(labels_,id);return true;}return commitSparse(labels_,id,std::move(next));}
  bool setStyle(RuntimeFieldId id,const RuntimeFieldStyle&s){if(!active(id))return false;if(s.empty()){removeSparse(styles_,id);return true;}detail::StoredStyle next;if(!detail::cloneStyle(next,s))return false;return commitSparse(styles_,id,std::move(next));}
  bool setStaticText(RuntimeFieldId id,const String&s){if(!active(id))return false;if(!s.length()){removeSparse(staticTexts_,id);return true;}detail::OwnedRuntimeString next;if(!detail::copyRuntimeString(next,s))return false;return commitSparse(staticTexts_,id,std::move(next));}
  bool promote(RuntimeFieldId id,size_t legacyIndex){auto*h=handle(id);if(!h||h->promoted||legacyIndex>UINT32_MAX)return false;switch(h->kind){
#define CM_CASE_RELEASE(k,t) case RuntimeFieldKind::k:{auto&r=pool<t>()[h->index];releaseRecord(r);using Record=typename std::remove_reference<decltype(r)>::type;r.~Record();new(&r) Record();break;}
    CM_EACH_KIND(CM_CASE_RELEASE)
#undef CM_CASE_RELEASE
    default:return false;}
    removeSparse(numericAlarms_,id);removeSparse(boolAlarms_,id);removeSparse(labels_,id);removeSparse(styles_,id);removeSparse(staticTexts_,id);h->promoted=true;h->index=static_cast<uint32_t>(legacyIndex);return true;
  }
  size_t textCount()const{size_t n=0;for(const auto&t:text_)if(t.uses)++n;return n;}
  bool materialize(RuntimeFieldId id,RuntimeFieldMeta& out)const {
    const auto v=view(id);if(!v.valid)return false;
    RuntimeFieldMeta next;
#define CM_COPY_STRING(n) if(!detail::copyRuntimeString(next.n,v.n))return false
    CM_COPY_STRING(group);CM_COPY_STRING(sourceGroup);CM_COPY_STRING(page);CM_COPY_STRING(key);CM_COPY_STRING(label);CM_COPY_STRING(unit);CM_COPY_STRING(onLabel);CM_COPY_STRING(offLabel);CM_COPY_STRING(staticValue);CM_COPY_STRING(card);
#undef CM_COPY_STRING
#define CM_COPY_SCALAR(n) next.n=v.n
    CM_COPY_SCALAR(precision);CM_COPY_SCALAR(order);CM_COPY_SCALAR(isBool);CM_COPY_SCALAR(isString);CM_COPY_SCALAR(isDivider);CM_COPY_SCALAR(isButton);CM_COPY_SCALAR(isCheckbox);CM_COPY_SCALAR(isStateButton);CM_COPY_SCALAR(isMomentaryButton);CM_COPY_SCALAR(isIntSlider);CM_COPY_SCALAR(isFloatSlider);CM_COPY_SCALAR(isIntInput);CM_COPY_SCALAR(isFloatInput);CM_COPY_SCALAR(hasAlarm);CM_COPY_SCALAR(alarmWhenTrue);CM_COPY_SCALAR(boolAlarmValue);CM_COPY_SCALAR(alarmMin);CM_COPY_SCALAR(alarmMax);CM_COPY_SCALAR(warnMin);CM_COPY_SCALAR(warnMax);CM_COPY_SCALAR(intMin);CM_COPY_SCALAR(intMax);CM_COPY_SCALAR(intInit);CM_COPY_SCALAR(floatMin);CM_COPY_SCALAR(floatMax);CM_COPY_SCALAR(floatInit);CM_COPY_SCALAR(floatPrecision);CM_COPY_SCALAR(initialState);
#undef CM_COPY_SCALAR
    // Legacy escape hatches keep the public vector layout and allocator behavior.
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
#endif
      next.style.rules.reserve(v.style.rules.size());
      for(size_t i=0;i<v.style.rules.size();++i){
        const auto r=v.style.rules[i];RuntimeStyleRule rule;
        if(!detail::copyRuntimeString(rule.target,r.target)||!detail::copyRuntimeString(rule.className,r.className))return false;
        rule.hasVisible=r.hasVisible;rule.visible=r.visible;rule.properties.reserve(r.properties.size());
        for(size_t j=0;j<r.properties.size();++j){const auto p=r.properties[j];RuntimeStyleProperty prop;if(!detail::copyRuntimeString(prop.key,p.key)||!detail::copyRuntimeString(prop.value,p.value))return false;rule.properties.push_back(std::move(prop));}
        next.style.rules.push_back(std::move(rule));
      }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    }catch(const std::bad_alloc&){return false;}
#endif
    out=std::move(next);return true;
  }
  bool canApplyLegacy(RuntimeFieldId id,const RuntimeFieldMeta&m)const{
    const auto v=view(id);if(!v.valid)return false;
#define CM_SAME_FLAG(n) if(m.n!=v.n)return false
    CM_SAME_FLAG(isBool);CM_SAME_FLAG(isString);CM_SAME_FLAG(isDivider);CM_SAME_FLAG(isButton);CM_SAME_FLAG(isCheckbox);CM_SAME_FLAG(isStateButton);CM_SAME_FLAG(isMomentaryButton);CM_SAME_FLAG(isIntSlider);CM_SAME_FLAG(isFloatSlider);CM_SAME_FLAG(isIntInput);CM_SAME_FLAG(isFloatInput);
#undef CM_SAME_FLAG
    const auto k=kind(id);
    if(!numeric(k)&&(m.unit.length()||m.precision!=2||m.alarmMin!=0||m.alarmMax!=0||m.warnMin!=0||m.warnMax!=0))return false;
    if((k==RuntimeFieldKind::Int||k==RuntimeFieldKind::IntSlider||k==RuntimeFieldKind::IntInput)&&m.precision!=0)return false;
    if(!boolean(k)&&(m.alarmWhenTrue||m.boolAlarmValue||m.onLabel.length()||m.offLabel.length()))return false;
    if(!boolean(k)&&!numeric(k)&&m.hasAlarm)return false;
    if(k!=RuntimeFieldKind::StateButton&&m.initialState)return false;
    if((k==RuntimeFieldKind::IntSlider||k==RuntimeFieldKind::IntInput)&&(m.intMin>m.intMax||m.intInit<m.intMin||m.intInit>m.intMax))return false;
    if((k==RuntimeFieldKind::FloatSlider||k==RuntimeFieldKind::FloatInput)&&(!(m.floatMin<=m.floatMax)||!(m.floatInit>=m.floatMin&&m.floatInit<=m.floatMax)))return false;
    if(k!=RuntimeFieldKind::IntSlider&&k!=RuntimeFieldKind::IntInput&&(m.intMin!=0||m.intMax!=100||m.intInit!=0))return false;
    if(k!=RuntimeFieldKind::FloatSlider&&k!=RuntimeFieldKind::FloatInput&&(m.floatMin!=0||m.floatMax!=1||m.floatInit!=0||m.floatPrecision!=2))return false;
    if((k==RuntimeFieldKind::FloatSlider||k==RuntimeFieldKind::FloatInput)&&m.floatPrecision!=m.precision)return false;
    return true;
  }
private:
  template<class T> bool applyLegacyTyped(RuntimeFieldId id,const RuntimeFieldMeta&m){
    T next;
#define CM_LEGACY_BASE(n) if(!detail::copyRuntimeString(next.base.n,m.n))return false
    CM_LEGACY_BASE(key);CM_LEGACY_BASE(label);CM_LEGACY_BASE(group);CM_LEGACY_BASE(sourceGroup);CM_LEGACY_BASE(page);CM_LEGACY_BASE(card);
#undef CM_LEGACY_BASE
    next.base.order=m.order;using P=typename detail::MetaTraits<T>::Payload;
    if constexpr(!std::is_same<P,detail::EmptyPayload>::value&&!std::is_same<P,detail::StatePayload>::value)if(!detail::copyRuntimeString(next.unit,m.unit))return false;
    if constexpr(std::is_same<P,detail::FloatPayload>::value||std::is_same<P,detail::FloatRangePayload>::value)next.precision=m.precision;
    if constexpr(std::is_same<P,detail::StatePayload>::value)next.initialState=m.initialState;
    if constexpr(std::is_same<P,detail::IntRangePayload>::value){next.intMin=m.intMin;next.intMax=m.intMax;next.intInit=m.intInit;}
    if constexpr(std::is_same<P,detail::FloatRangePayload>::value){next.floatMin=m.floatMin;next.floatMax=m.floatMax;next.floatInit=m.floatInit;}
    return replace(id,next);
  }
public:
  bool applyLegacy(RuntimeFieldId id,const RuntimeFieldMeta&m){
    if(!canApplyLegacy(id,m))return false;
    detail::StoredStyle style;detail::StoredLabels labels;detail::OwnedRuntimeString staticText;
    if(!detail::cloneStyle(style,m.style)||!detail::copyRuntimeString(labels.onLabel,m.onLabel)||!detail::copyRuntimeString(labels.offLabel,m.offLabel)||!detail::copyRuntimeString(staticText,m.staticValue))return false;
    const auto k=kind(id);const bool na=numeric(k)&&(m.hasAlarm||m.alarmMin!=0||m.alarmMax!=0||m.warnMin!=0||m.warnMax!=0);
    const bool ba=boolean(k)&&(m.hasAlarm||m.alarmWhenTrue||m.boolAlarmValue);
    const bool hasLabels=m.onLabel.length()||m.offLabel.length();
    if((na&&!sparse(numericAlarms_,id)&&!numericAlarms_.ensureOne())||(ba&&!sparse(boolAlarms_,id)&&!boolAlarms_.ensureOne())||(hasLabels&&!sparse(labels_,id)&&!labels_.ensureOne())||(!style.empty()&&!sparse(styles_,id)&&!styles_.ensureOne())||(staticText.length()&&!sparse(staticTexts_,id)&&!staticTexts_.ensureOne()))return false;
    bool replaced=false;
    switch(k){
#define CM_CASE_APPLY(k,t) case RuntimeFieldKind::k:replaced=applyLegacyTyped<t>(id,m);break;
      CM_EACH_KIND(CM_CASE_APPLY)
#undef CM_CASE_APPLY
      default:return false;
    }
    if(!replaced)return false;
    if(na)commitSparse(numericAlarms_,id,RuntimeNumericAlarm{m.hasAlarm,m.alarmMin,m.alarmMax,m.warnMin,m.warnMax});else removeSparse(numericAlarms_,id);
    if(ba)commitSparse(boolAlarms_,id,RuntimeBoolAlarm{m.hasAlarm,m.alarmWhenTrue,m.boolAlarmValue});else removeSparse(boolAlarms_,id);
    if(hasLabels)commitSparse(labels_,id,std::move(labels));else removeSparse(labels_,id);
    if(!style.empty())commitSparse(styles_,id,std::move(style));else removeSparse(styles_,id);
    if(staticText.length())commitSparse(staticTexts_,id,std::move(staticText));else removeSparse(staticTexts_,id);
    return true;
  }
  struct Stats { size_t retainedBytes=0, poolCapacityBytes=0, textCapacityBytes=0, handleCapacityBytes=0, sparseCapacityBytes=0, activeTextCount=0; };
  Stats stats()const{
    Stats s;s.retainedBytes=sizeof(*this);s.handleCapacityBytes=handles_.capacity()*sizeof(Handle);s.textCapacityBytes=text_.capacity()*sizeof(TextEntry);
    const auto stringBytes=[](const detail::OwnedRuntimeString& str)->size_t {return str.allocatedBytes();};
    for(const auto&t:text_)s.textCapacityBytes+=stringBytes(t.value);
#define CM_POOL_BYTES(k,t) {const auto&p=pool<t>();s.poolCapacityBytes+=p.capacity()*sizeof(p[0]);for(const auto&r:p){s.poolCapacityBytes+=stringBytes(r.base.key)+stringBytes(r.base.label);}}
    CM_EACH_KIND(CM_POOL_BYTES)
#undef CM_POOL_BYTES
    s.sparseCapacityBytes=numericAlarms_.capacity()*sizeof(Sparse<RuntimeNumericAlarm>)+boolAlarms_.capacity()*sizeof(Sparse<RuntimeBoolAlarm>)+labels_.capacity()*sizeof(Sparse<detail::StoredLabels>)+styles_.capacity()*sizeof(Sparse<detail::StoredStyle>)+staticTexts_.capacity()*sizeof(Sparse<detail::OwnedRuntimeString>);
    for(const auto&e:labels_)s.sparseCapacityBytes+=stringBytes(e.value.onLabel)+stringBytes(e.value.offLabel);
    for(const auto&e:staticTexts_)s.sparseCapacityBytes+=stringBytes(e.value);
    for(const auto&e:styles_){s.sparseCapacityBytes+=e.value.rules.capacity()*sizeof(detail::StoredRule);for(const auto&r:e.value.rules){s.sparseCapacityBytes+=stringBytes(r.target)+stringBytes(r.className)+r.properties.capacity()*sizeof(detail::StoredProperty);for(const auto&p:r.properties)s.sparseCapacityBytes+=stringBytes(p.key)+stringBytes(p.value);}}
    s.retainedBytes+=s.poolCapacityBytes+s.textCapacityBytes+s.handleCapacityBytes+s.sparseCapacityBytes;s.activeTextCount=textCount();return s;
  }
  size_t retainedBytes()const{return stats().retainedBytes;}
#undef CM_EACH_KIND
};
} // namespace cm
