# Code Review Style Guide

This file contains style and formatting rules that the review subagent must check and fix. These are mechanical issues that should always be caught during code review.

## Empty line before closing brace

Always add an empty line before the closing brace of a **class** (which has one or more sections like `public:` / `private:`). Plain **structs** with just data members do NOT get a trailing empty line — they are compact: `struct Foo { data lines; };`.

```cpp
// BAD:
class MyClass {
public:
    void foo();

private:
    int _value = 0;
};

// GOOD:
class MyClass {
public:
    void foo();

private:
    int _value = 0;

};
```

## Multi-line expressions — operators at the start of continuation lines

When splitting an expression across multiple lines, place operators (like `&&`, `||`, `;`, `+`, etc.) at the **beginning** of continuation lines, not at the end of the previous line. This makes it immediately obvious from the left edge whether a line is a continuation or new code.

```cpp
// BAD - continuation looks like scope code:
if (const auto &lottie = animation->lottie;
	lottie && lottie->valid() && lottie->framesCount() > 1) {
	lottie->animate([=] {

// GOOD - semicolon at start signals continuation:
if (const auto &lottie = animation->lottie
	; lottie && lottie->valid() && lottie->framesCount() > 1) {
	lottie->animate([=] {

// BAD - trailing && makes next line look like independent code:
if (veryLongExpression() &&
	anotherLongExpression() &&
	anotherOne()) {
	doSomething();

// GOOD - leading && clearly marks continuation:
if (veryLongExpression()
	&& anotherLongExpression()
	&& anotherOne()) {
	doSomething();
```

## Minimize type checks — prefer direct cast over is + as

Don't check a type and then cast — just cast and check for null. `asUser()` already returns `nullptr` when the peer is not a user, so calling `isUser()` first is redundant. The same applies to `asChannel()`, `asChat()`, etc.

```cpp
// BAD - redundant isUser() check, then asUser():
if (peer && peer->isUser()) {
	peer->asUser()->setNoForwardFlags(

// GOOD - just cast and null-check:
if (const auto user = peer->asUser()) {
	user->setNoForwardFlags(
```

When you need a specific subtype, look up the specific subtype directly instead of loading a generic type and then casting:

```cpp
// BAD - loads generic peer, then casts:
if (const auto peer = session().data().peerLoaded(peerId)
	; peer && peer->isUser()) {
	peer->asUser()->setNoForwardFlags(

// GOOD - look up the specific subtype directly:
const auto userId = peerToUser(peerId);
if (const auto user = session().data().userLoaded(userId)) {
	user->setNoForwardFlags(
```

Avoid C++17 `if` with initializer (`;` inside the condition) when the code can be written more clearly with simple nested `if` statements or by extracting the value beforehand:

```cpp
// BAD - complex if-with-initializer:
if (const auto peer = session().data().peerLoaded(peerId)
	; peer && peer->isUser()) {

// GOOD - simple nested ifs when direct lookup isn't available:
if (const auto peer = session().data().peerLoaded(peerId)) {
	if (const auto user = peer->asUser()) {

## Always initialize variables of basic types

Never leave variables of basic types (`int`, `float`, `bool`, pointers, etc.) uninitialized. Custom types with constructors are fine — they initialize themselves. But for any basic type, always provide a default value (`= 0`, `= false`, `= nullptr`, etc.). This applies especially to class fields, where uninitialized members are a persistent source of bugs.

The only exception is performance-critical hot paths where you can prove no read-from-uninitialized-memory occurs. For class fields there is no such exception — always initialize.

```cpp
// BAD:
int _bulletLeft;
int _bulletTop;
bool _expanded;
SomeType *_pointer;

// GOOD:
int _bulletLeft = 0;
int _bulletTop = 0;
bool _expanded = false;
SomeType *_pointer = nullptr;
```

## Use tr:: projections for TextWithEntities

Inside `tr::lng_...()` calls, always use the `tr::` projection helpers instead of their `Ui::Text::` equivalents. The `tr::` helpers are shorter and work uniformly as both placeholder wrappers and final projectors.

| Instead of | Use |
|---|---|
| `Ui::Text::Bold(x)` | `tr::bold(x)` |
| `Ui::Text::Italic(x)` | `tr::italic(x)` |
| `Ui::Text::RichLangValue` | `tr::rich` |
| `Ui::Text::WithEntities` | `tr::marked` |

```cpp
// BAD - verbose Ui::Text:: functions:
tr::lng_some_key(
    tr::now,
    lt_name,
    Ui::Text::Bold(name),
    lt_group,
    Ui::Text::Bold(group),
    Ui::Text::RichLangValue)

// GOOD - concise tr:: helpers:
tr::lng_some_key(
    tr::now,
    lt_name,
    tr::bold(name),
    lt_group,
    tr::bold(group),
    tr::rich)
```

Also use `tr::marked()` as the standard way to create `TextWithEntities` — not just as a projector:

```cpp
// BAD - verbose constructor:
auto text = TextWithEntities();
auto text = TextWithEntities{ u"hello"_q };
auto text = TextWithEntities().append(u"hello"_q);

// GOOD - concise:
auto text = tr::marked();
auto text = tr::marked(u"hello"_q);
```

## Multi-line calls — one argument per line

When a function call doesn't fit on one line, put each argument on its own line. Don't group "logical pairs" on the same line — it creates inconsistent line lengths and makes diffs noisier.

```cpp
// BAD - pairs of arguments sharing lines:
tr::lng_some_key(
    tr::now,
    lt_name, tr::bold(name),
    lt_group, tr::bold(group),
    tr::rich)

// GOOD - one argument per line:
tr::lng_some_key(
    tr::now,
    lt_name,
    tr::bold(name),
    lt_group,
    tr::bold(group),
    tr::rich)

// Single-line is fine when everything fits:
auto text = tr::lng_settings_title(tr::now);
```

## std::optional access — avoid value()

Do not call `std::optional::value()` because it throws an exception that is not available on older macOS targets. Use `has_value()`, `value_or()`, `operator bool()`, or `operator*` instead.

## Sort includes alphabetically, nested folders first

After the file's own header, sort `#include` directives alphabetically with two special rules:

1. **Nested folders before files** in the same directory — like Finder / File Explorer (folders first, then files). E.g. `ui/controls/button.h` sorts before `ui/abstract_button.h`.
2. **Style includes (`styles/style_*.h`) always go last**, separated from the rest.

```cpp
// BAD - arbitrary order, style mixed in:
#include "media/audio/media_audio.h"
#include "styles/style_media_player.h"
#include "data/data_document.h"
#include "apiwrap.h"

// GOOD - alphabetical, folders first, styles last:
#include "apiwrap.h"
#include "data/data_document.h"
#include "media/audio/media_audio.h"

#include "styles/style_media_player.h"
```

## Use C++17 nested namespace syntax

Use `namespace A::B {` instead of nesting `namespace A { namespace B {`. The closing comment mirrors the opening: `} // namespace A::B`.

```cpp
// BAD - old-style nesting:
namespace Media {
namespace Player {
...
} // namespace Player
} // namespace Media

// GOOD - C++17 nested:
namespace Media::Player {
...
} // namespace Media::Player
```

## Merge consecutive branches with identical bodies

When two or more consecutive `if` / `else if` branches execute the same code, combine their conditions into a single branch.

```cpp
// BAD - duplicated body:
if (!document) {
    finalize();
    return;
}
if (!document->isSong()) {
    finalize();
    return;
}

// GOOD - combined:
if (!document || !document->isSong()) {
    finalize();
    return;
}
```

## Use base::take for read-and-reset

When you need to read a variable's current value and reset it in one step, use `base::take(var)` instead of manually copying and clearing. `base::take` returns the old value and resets the variable to its default-constructed state.

```cpp
// BAD - manual read + reset:
if (_playing) {
    _listenedMs += crl::now() - _playStartedAt;
    _playing = false;
}

// GOOD:
if (base::take(_playing)) {
    _listenedMs += crl::now() - _playStartedAt;
}

// BAD - copy fields then clear them one by one:
const auto document = _document;
const auto contextId = _contextId;
_document = nullptr;
_listenedMs = 0;
if (!document) {
    return;
}

// GOOD - take everything upfront, then validate:
const auto document = base::take(_document);
const auto contextId = base::take(_contextId);
const auto duration = static_cast<int>(base::take(_listenedMs) / 1000);
if (!document || duration <= 0) {
    return;
}
```

## Don't wrap tr:: lang keys in rpl::single

`tr::lng_*()` (without `tr::now`) already returns an `rpl::producer`. Wrapping a snapshot in `rpl::single()` defeats live language switching — the value is captured once and never updates. Just call the lang key without `tr::now`.

```cpp
// BAD - frozen snapshot, won't update on language change:
rpl::single(tr::lng_ai_compose_title(tr::now))

// GOOD - live producer that updates automatically:
tr::lng_ai_compose_title()
```

## Extract method definitions from local classes

When defining local classes (e.g. in anonymous namespaces), keep the class body compact — only declarations. Put all method definitions **after** all class definitions. This avoids unnecessary nesting inside the class body and keeps methods at the same indentation level as free functions.

```cpp
// BAD - methods defined inline, adding a nesting level:
class MyWidget final : public Ui::RpWidget {
public:
    MyWidget(QWidget *parent)
    : RpWidget(parent) {
        // ... 20 lines of setup
    }

    void setActive(bool active) {
        _active = active;
        update();
    }

protected:
    void paintEvent(QPaintEvent *e) override {
        // ... 30 lines of painting
    }

private:
    bool _active = false;

};

// GOOD - class is a compact declaration, methods defined after:
class MyWidget final : public Ui::RpWidget {
public:
    MyWidget(QWidget *parent, QString label);

    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    bool _active = false;

};

MyWidget::MyWidget(QWidget *parent, QString label)
: RpWidget(parent) {
    // ... 20 lines of setup
}

void MyWidget::setActive(bool active) {
    _active = active;
    update();
}

void MyWidget::paintEvent(QPaintEvent *e) {
    // ... 30 lines of painting
}
```

When there are multiple local classes, put **all class definitions first**, then **all method definitions** after. This keeps the declarations readable as an overview.

## Use RAII for resource cleanup

When working with raw resources (Win32 HANDLEs, file descriptors, COM objects), use `gsl::finally` or a dedicated RAII wrapper for cleanup instead of calling release functions manually. Manual cleanup breaks when early returns are added later.

```cpp
// BAD - manual cleanup, fragile with early returns:
const auto snapshot = CreateToolhelp32Snapshot(...);
if (snapshot != INVALID_HANDLE_VALUE) {
	// ... logic that might grow early returns ...
	CloseHandle(snapshot);
}

// GOOD - RAII guard, cleanup runs on any exit path:
const auto snapshot = CreateToolhelp32Snapshot(...);
if (snapshot == INVALID_HANDLE_VALUE) {
	return;
}
const auto guard = gsl::finally([&] {
	CloseHandle(snapshot);
});
// ... logic, early returns are safe ...
```

## Extract substantial logic from lambdas

When a lambda grows beyond a few lines of self-contained logic, extract it into a named function (free function in anonymous namespace, or a private method). Lambdas should primarily be glue — captures, dispatch, short transforms. This applies when the lambda's captures are minimal and can easily become function parameters. When a lambda captures many variables from its surrounding context, it may be cleaner to keep it inline.

```cpp
// BAD - substantial logic buried in a lambda:
crl::async([=] {
	auto found = false;
	auto pe = PROCESSENTRY32();
	pe.dwSize = sizeof(PROCESSENTRY32);
	const auto snapshot = CreateToolhelp32Snapshot(...);
	if (snapshot != INVALID_HANDLE_VALUE) {
		for (...) {
			if (/* match */) {
				found = true;
				break;
			}
		}
		CloseHandle(snapshot);
	}
	crl::on_main(weak, [=] { handle(found); });
});

// GOOD - logic extracted, lambda is just glue:
crl::async([=] {
	const auto found = FindRunningReader();
	crl::on_main(weak, [=] { handle(found); });
});
```

## Data-driven matching over chained conditions

When comparing a value against multiple known constants, store them in a collection and loop instead of chaining `||` conditions. Easier to extend, less repetition, and reads as data rather than logic.

```cpp
// BAD - repetitive chain, hard to extend:
if (_wcsicmp(name, L"Narrator.exe") == 0
	|| _wcsicmp(name, L"nvda.exe") == 0
	|| _wcsicmp(name, L"jfw.exe") == 0
	|| _wcsicmp(name, L"Zt.exe") == 0) {

// GOOD - data-driven, easy to extend:
const auto list = std::array{
	L"Narrator.exe",
	L"nvda.exe",
	L"jfw.exe",
	L"Zt.exe",
};
for (const auto &entry : list) {
	if (_wcsicmp(name, entry) == 0) {
		return true;
	}
}
```

## Use !isHidden() for logic checks, not isVisible()

When you call `show()` / `hide()` / `setVisible()` on a widget and later branch on that state, always check `!isHidden()` (the widget's own flag) — never `isVisible()`. `isVisible()` returns `true` only when the widget **and every ancestor** are visible, so it silently returns `false` during parent show-animations, before the parent is laid out, etc. `isHidden()` reflects exactly the flag you set.

```cpp
// BAD — breaks when parent is still animating / not yet shown:
child->setVisible(true);
// ... later, in resizeGetHeight or similar:
if (child->isVisible()) {   // false if parent isn't visible yet!
    child->moveToRight(x, y, w);
}

// GOOD — checks the widget's own state:
if (!child->isHidden()) {
    child->moveToRight(x, y, w);
}
```

The same applies to any logic that depends on a previous `show()`/`hide()` call: skip blocks, layout branches, opacity decisions, etc.

## Consolidate make_state calls into a single State struct

Every `make_state` is a separate heap allocation. When a function needs multiple pieces of lambda-captured mutable state, define a local `struct State` with all fields and call `make_state<State>()` once, then capture the resulting pointer everywhere.

```cpp
// BAD - two allocations:
const auto shown = lifetime.make_state<bool>(false);
const auto count = lifetime.make_state<int>(0);

// GOOD - one allocation:
struct State {
	bool shown = false;
	int count = 0;
};
const auto state = lifetime.make_state<State>();
```

## Use trailing return type when the return type doesn't fit on one line

When a function's return type is long enough that the declaration would need a line break between the return type and the function name, use trailing return type syntax (`auto ... -> Type`) to keep the function name on the opening line.

```cpp
// BAD - return type orphaned on its own line:
not_null<HistoryView::Controls::ComposeAiButton*>
SetupCaptionAiButton(SetupCaptionAiButtonArgs &&args);

// GOOD - trailing return type keeps name visible:
auto SetupCaptionAiButton(SetupCaptionAiButtonArgs &&args)
-> not_null<HistoryView::Controls::ComposeAiButton*>;
```

## Mind data structure sizes and alignment

When adding fields to a class or struct, consider the memory layout. A standalone `bool` between two pointer-sized fields wastes 7 bytes to alignment padding. Review new fields for packing opportunities:

- If the struct already has bitfields, pack new boolean flags as `: 1` members rather than standalone `bool`.
- If alignment leaves a gap (e.g., an `int` followed by a pointer), consider whether a new small field can fill it.
- For classes instantiated in large quantities (per-message, per-element, per-row), every wasted byte is multiplied thousands of times.

```cpp
// BAD - standalone bool adds 8 bytes (1 + 7 padding) before the next pointer:
mutable bool _myFlag = false;
mutable std::unique_ptr<Foo> _foo;

// GOOD - packed into existing bitfield group, no extra bytes:
mutable uint32 _myFlag : 1 = 0;
```

## Static member functions use PascalCase

Non-static member functions use camelCase (`startBatch`, `finalize`). Static member functions use PascalCase (`ShouldTrack`, `Parse`, `Create`), matching the convention for free functions.

```cpp
// BAD - camelCase for static method:
[[nodiscard]] static bool shouldTrack(not_null<HistoryItem*> item);

// GOOD - PascalCase for static method:
[[nodiscard]] static bool ShouldTrack(not_null<HistoryItem*> item);
```
