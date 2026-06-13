/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"
#include "iv/editor/iv_editor_state.h"
#include "iv/markdown/iv_markdown_article.h"
#include "ui/style/style_core_types.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/rp_widget.h"
#include "rpl/lifetime.h"

#include <rpl/event_stream.h>

#include <memory>
#include <optional>
#include <vector>

class QEvent;
class QContextMenuEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMimeData;
class QMenu;
class QObject;
class QTouchEvent;
class QWheelEvent;

namespace Ui {
class ChatStyle;
class ChatTheme;
class InputField;
class PopupMenu;
} // namespace Ui

namespace style {
struct InputField;
struct Markdown;
} // namespace style

class PeerData;
class DocumentData;

namespace Main {
class Session;
class SessionShow;
} // namespace Main

namespace Iv::Editor {

struct WidgetServices {
	not_null<Main::Session*> session;
	std::shared_ptr<Main::SessionShow> show;
	not_null<QWidget*> outer;
	Fn<bool()> customEmojiPaused;
	rpl::producer<> imeCompositionStarts;
};

class Widget final : public Ui::RpWidget {
public:
	Widget(
		QWidget *parent,
		WidgetServices services,
		not_null<PeerData*> peer,
		std::shared_ptr<State> state,
		Fn<void(RichMessageLimitError)> showLimitToast = {});

	void activateInitialNode();
	void activateSegment(int segmentIndex, int cursorOffset);
	[[nodiscard]] State::ApplyResult commitInlineField();
	void refreshPreparedContent();
	void refreshPreparedLeafAtActiveSource();
	void applyExternalRichPageMutation(Fn<bool(RichPage&)> mutation);
	void syncInlineFieldGeometry();
	void insertBlock(State::InsertAction action);
	void insertPreparedBlock(RichPage::Block block);
	void insertPreparedBlocks(std::vector<RichPage::Block> blocks);
	void insertHeading1();
	void insertBlockquote();
	void insertEmoji(EmojiPtr emoji);
	void insertCustomEmoji(not_null<DocumentData*> document);
	void setInlineFieldExternalInteractionActive(bool active);

	int resizeGetHeight(int newWidth) override;

protected:
	bool eventFilter(QObject *object, QEvent *event) override;
	bool eventHook(QEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
	void focusInEvent(QFocusEvent *e) override;
	bool focusNextPrevChild(bool next) override;
	void keyPressEvent(QKeyEvent *e) override;
	void inputMethodEvent(QInputMethodEvent *e) override;
	QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void visibleTopBottomUpdated(int visibleTop, int visibleBottom) override;
	void wheelEvent(QWheelEvent *e) override;

private:
	struct InlineFieldStyleData {
		const style::TextStyle *textStyle = nullptr;
		int lineHeight = 0;
		style::color textFg;
		QColor textMarkBg;
		style::align align = style::al_left;
		bool italic = false;
	};

	struct InlineFieldStyleKey {
		style::font font;
		int lineHeight = 0;
		style::color textFg;
		style::align align = style::al_left;

		friend inline bool operator==(
				const InlineFieldStyleKey &a,
				const InlineFieldStyleKey &b) {
			return (a.font == b.font)
				&& (a.lineHeight == b.lineHeight)
				&& (a.textFg == b.textFg)
				&& (a.align == b.align);
		}

		friend inline bool operator!=(
				const InlineFieldStyleKey &a,
				const InlineFieldStyleKey &b) {
			return !(a == b);
		}
	};

	struct CachedInlineFieldStyle {
		InlineFieldStyleKey key;
		std::shared_ptr<style::InputField> style;
		std::shared_ptr<style::owned_color> ownedTextFg;
		std::shared_ptr<style::owned_color> ownedTextMarkBg;
	};

	enum class DragSelectionMode {
		None,
		Text,
		Structural,
	};

	struct ArticleSelectionDrag {
		bool active = false;
		bool fromField = false;
		bool startedBelow = false;
		bool codeHeader = false;
		QPoint pressPoint;
		Markdown::PreparedEditHit anchorHit;
		int textSegment = -1;
		int textOffset = 0;
		DragSelectionMode mode = DragSelectionMode::None;
	};

	enum class HorizontalScrollDrag {
		None,
		Mouse,
		Touch,
	};

	struct BoundarySelectionOrigin {
		int ordinal = -1;
		bool forward = false;

		friend inline bool operator==(
				const BoundarySelectionOrigin &a,
				const BoundarySelectionOrigin &b) {
			return (a.ordinal == b.ordinal)
				&& (a.forward == b.forward);
		}
	};

	struct HistoryLeafSelection {
		State::LeafPath leaf;
		int anchorOffset = 0;
		int cursorOffset = 0;

		friend inline bool operator==(
				const HistoryLeafSelection &a,
				const HistoryLeafSelection &b) {
			return (a.leaf == b.leaf)
				&& (a.anchorOffset == b.anchorOffset)
				&& (a.cursorOffset == b.cursorOffset);
		}
	};

	struct HistoryViewState {
		std::optional<HistoryLeafSelection> leafSelection;
		std::optional<Markdown::PreparedEditSelection> structuralSelection;
		std::optional<BoundarySelectionOrigin> boundarySelectionOrigin;

		friend inline bool operator==(
				const HistoryViewState &a,
				const HistoryViewState &b) {
			return (a.leafSelection == b.leafSelection)
				&& (a.structuralSelection == b.structuralSelection)
				&& (a.boundarySelectionOrigin
					== b.boundarySelectionOrigin);
		}
	};

	struct HistoryEntry {
		State::Snapshot snapshot;
		HistoryViewState viewState;
	};

	struct MutationTransactionResult {
		State::ApplyResult committed = State::ApplyResult::Unchanged;
		bool changed = false;
		bool failed = false;
	};

	struct RetainedLeafField {
		int historyIndex = -1;
		uint64 retainToken = 0;
		State::LeafPath leaf;
		State::FieldMode mode = State::FieldMode::Rich;
		std::optional<InlineFieldStyleKey> styleKey;
		base::unique_qptr<Ui::InputField> field;
	};

	void setDocument(const Markdown::MarkdownArticleContent &prepared);
	void activateTextOrdinal(int ordinal, int cursorOffset);
	void activateTextOrdinal(int ordinal, int selectionFrom, int selectionTo);
	[[nodiscard]] Markdown::MarkdownArticleTextLeafStyle
	inlineFieldStyleForSegment(int segmentIndex) const;
	[[nodiscard]] const CachedInlineFieldStyle &inlineFieldStyleFor(
		const Markdown::MarkdownArticleTextLeafStyle &style);
	[[nodiscard]] const CachedInlineFieldStyle &inlineFieldStyleFor(
		const InlineFieldStyleData &data);
	[[nodiscard]] InlineFieldStyleData normalizedInlineFieldStyle(
		const Markdown::MarkdownArticleTextLeafStyle &style) const;
	[[nodiscard]] InlineFieldStyleKey inlineFieldStyleKey(
		const InlineFieldStyleData &data) const;
	void refreshInlineFieldTextColorOverride();
	[[nodiscard]] std::optional<QColor> activeQuoteCaptionColor();
	[[nodiscard]] std::optional<QColor> activeQuotePlaceholderColor();
	void ensureInlineFieldForSegment(int segmentIndex);
	void setupInlineField();
	void ensureInlineFieldCreated();
	void recreateInlineField(const style::InputField &st);
	void refreshInlineFieldPlaceholder();
	void refreshInlineFieldPlaceholderColor();
	void activateTrailingParagraph();
	void setInlineFieldFromActiveState(int selectionFrom, int selectionTo);
	void revertInlineFieldToState();
	[[nodiscard]] std::optional<State::ActiveTextInsertContext>
	activeTextInsertContext() const;
	[[nodiscard]] int richOffsetForFieldOffset(
		const TextWithEntities &text,
		int offset) const;
	[[nodiscard]] State::ApplyResult applyFieldTextToState();
	bool showLastLimitToast();
	void hideInlineField();
	void acceptInlineField();
	void hideInlineFieldAndRefresh();
	void activateTextOrdinalAtEnd(int ordinal);
	[[nodiscard]] bool redirectKeyToField(QKeyEvent *e) const;
	[[nodiscard]] bool redirectImeToField() const;
	[[nodiscard]] bool prepareFieldForInput();
	[[nodiscard]] std::optional<int> removeCurrentStructuralSelection(
		bool forward);
	[[nodiscard]] bool replayKeyIntoField(QKeyEvent *e);
	[[nodiscard]] bool replayImeIntoField(QInputMethodEvent *e);
	[[nodiscard]] bool handleTabNavigation(QKeyEvent *e);
	[[nodiscard]] bool handleClipboardKey(QKeyEvent *e);
	[[nodiscard]] bool handleFieldKey(QKeyEvent *e);
	void copyCurrentSelectionToClipboard();
	[[nodiscard]] TextForMimeData currentSelectionTextForClipboard() const;
	void pasteStructuredClipboardData(const ClipboardData &data);
	[[nodiscard]] bool handleIvClipboardMime(
		not_null<const QMimeData*> data,
		Ui::InputField::MimeAction action);
	[[nodiscard]] bool moveBoundary(bool forward, bool allowTrailing);
	[[nodiscard]] bool moveBoundaryAfterCommit(
		State::ApplyResult committed,
		bool forward,
		bool allowTrailing,
		bool *mutated = nullptr);
	[[nodiscard]] bool moveTabBoundary(bool forward);
	[[nodiscard]] bool removeBoundaryOwner(bool forward);
	void ensurePendingActivation();
	void updateInlineFieldHeightOverride();
	void clearDisplayMathEditSession();
	void clearInlineFieldEditSession();
	[[nodiscard]] HistoryViewState captureHistoryViewState() const;
	[[nodiscard]] HistoryEntry captureHistoryEntry() const;
	void restoreHistoryEntry(const HistoryEntry &entry);
	[[nodiscard]] static bool mutationTransactionChanged(bool changed);
	[[nodiscard]] static bool mutationTransactionChanged(
		State::ApplyResult result);
	[[nodiscard]] static bool mutationTransactionChanged(
		const MutationTransactionResult &result);
	void finishMutationTransaction(
		const HistoryEntry &before,
		bool changed,
		int beforeHistoryIndex,
		uint64 beforeRetainToken);
	template <typename Callback>
	auto recordMutationTransaction(Callback &&callback)
	-> decltype(callback()) {
		const auto before = captureHistoryEntry();
		const auto beforeHistoryIndex = _historyIndex;
		const auto beforeRetainToken = _retainedLeafFieldToken;
		auto result = callback();
		finishMutationTransaction(
			before,
			mutationTransactionChanged(result),
			beforeHistoryIndex,
			beforeRetainToken);
		return result;
	}
	void truncateHistoryRedo();
	[[nodiscard]] bool activeInlineFieldTextMatchesState() const;
	[[nodiscard]] bool canPerformFieldUndoRedo(bool redo) const;
	[[nodiscard]] bool canPerformHistoryUndoRedo(bool redo) const;
	[[nodiscard]] bool canPerformUndoRedo(bool redo) const;
	[[nodiscard]] bool handleUndoRedoShortcut(QKeyEvent *e);
	[[nodiscard]] bool handleSelectAllShortcut(QKeyEvent *e);
	[[nodiscard]] bool performFieldUndoRedo(bool redo);
	void performUndoRedo(bool redo, bool allowFieldLocal = true);
	void clearFieldUndoRedoNoopState();
	void retainActiveLeafField();
	[[nodiscard]] base::unique_qptr<Ui::InputField> reviveRetainedLeafField(
		int historyIndex,
		const State::LeafPath &leaf,
		State::FieldMode mode,
		const InlineFieldStyleKey &styleKey);
	void pruneRetainedLeafFields();
	void removeRetainedLeafFieldsAfter(int historyIndex);
	void moveRetainedLeafFields(
		int fromHistoryIndex,
		int toHistoryIndex,
		uint64 afterRetainToken);
	void relayoutCurrentContent();
	void refreshAfterInlineFieldCommit(State::ApplyResult committed);
	void ensureArticleLayoutForInlineField(int width);
	void syncArticleVisibleTopBottom();
	void syncInlineFieldGeometry(int width);
	[[nodiscard]] QRect activeInlineFieldRevealRect() const;
	[[nodiscard]] QRect mapFieldLocalRectToScrollContent(
		QWidget *inner,
		QRect rect) const;
	void revealActiveInlineField();
	void clearSelection();
	void clearTextSelection();
	void clearStructuralSelection();
	void setStructuralSelection(
		Markdown::PreparedEditSelection selection,
		std::optional<BoundarySelectionOrigin> origin = std::nullopt);
	[[nodiscard]] bool hasStructuralSelection() const;
	void startArticleSelection(
		QPoint pressPoint,
		const Markdown::MarkdownArticleHitTestResult &hit,
		const Markdown::PreparedEditHit &editHit,
		bool fromField = false,
		bool startedBelow = false);
	void updateArticleSelection(
		QPoint articlePoint,
		const Markdown::MarkdownArticleHitTestResult &hit,
		const Markdown::PreparedEditHit &editHit);
	void finishArticleSelection();
	[[nodiscard]] bool handleStructuralSelectionKey(QKeyEvent *e);
	[[nodiscard]] bool handleFieldContextMenuEvent(
		QObject *object,
		QContextMenuEvent *e);
	[[nodiscard]] bool handleFieldMouseEvent(QEvent *event);
	[[nodiscard]] bool handleHorizontalScrollWheel(
		QWheelEvent *e,
		QPoint articlePoint);
	[[nodiscard]] std::optional<Markdown::PreparedEditTableCellSource>
	activeTableCellSourceAt(
		QObject *object,
		const QContextMenuEvent &e) const;
	[[nodiscard]] Markdown::PreparedEditTableCellRange
	effectiveTableRangeForCell(
		const Markdown::PreparedEditTableCellSource &source);
	void showTableContextMenu(
		const Markdown::PreparedEditTableCellRange &range,
		QPoint globalPos);
	void fillTableChangeMenu(
		not_null<Ui::PopupMenu*> menu,
		const Markdown::PreparedEditTableCellRange &range);
	void applyTableChange(Fn<bool()> change);
	[[nodiscard]] Markdown::PreparedEditSelection structuralSelectionFromHits(
		const Markdown::PreparedEditHit &anchor,
		const Markdown::PreparedEditHit &focus) const;
	[[nodiscard]] int editableOrdinalForSegment(int segmentIndex) const;
	[[nodiscard]] int segmentIndexForEditableOrdinal(int ordinal) const;
	[[nodiscard]] QPoint articleTopLeft() const;
	[[nodiscard]] int articleWidth(int outerWidth) const;
	void touchEvent(QTouchEvent *e);
	[[nodiscard]] QRect fieldOuterRectForSegment(int segmentIndex) const;
	[[nodiscard]] QRect outerEditableSegmentRect(int segmentIndex) const;
	[[nodiscard]] Markdown::MarkdownArticlePaintContext textPaintContext(
		QRect clip);

	const not_null<Main::Session*> _session;
	const std::shared_ptr<Main::SessionShow> _show;
	const not_null<QWidget*> _outer;
	const Fn<bool()> _customEmojiPaused;
	const not_null<PeerData*> _peer;
	const std::shared_ptr<State> _state;
	const Fn<void(RichMessageLimitError)> _showLimitToast;
	std::shared_ptr<style::Markdown> _articleStyle;
	std::shared_ptr<Markdown::MarkdownArticle> _article;
	base::unique_qptr<Ui::InputField> _field;
	rpl::event_stream<Ui::InputField::ExtendedContextMenu>
		_fieldContextMenuRequests;
	std::unique_ptr<Ui::ChatTheme> _theme;
	std::unique_ptr<Ui::ChatStyle> _style;
	std::vector<Ui::Text::SpecialColor> _highlightColors;
	rpl::lifetime _highlightReadyLifetime;
	std::vector<CachedInlineFieldStyle> _fieldStyles;
	std::optional<style::owned_color> _inlineFieldTextColorOverride;
	std::optional<style::owned_color> _inlineFieldPlaceholderColorOverride;
	std::optional<InlineFieldStyleKey> _activeFieldStyleKey;
	std::optional<State::LeafPath> _fieldLeaf;
	State::FieldMode _fieldMode = State::FieldMode::Rich;
	int _articleHeight = 0;
	int _activeOrdinal = -1;
	int _activeSegmentIndex = -1;
	bool _activeSegmentIsDisplayMath = false;
	int _activeDisplayMathBaselineHeight = 0;
	int _pendingOrdinal = -1;
	int _pendingCursorOffset = 0;
	std::vector<HistoryEntry> _history;
	int _historyIndex = -1;
	std::vector<RetainedLeafField> _retainedLeafFields;
	uint64 _retainedLeafFieldToken = 0;
	std::optional<int> _retainingFieldHistoryIndexOverride;
	std::optional<bool> _restoringHistoryRedo;
	bool _restoringHistory = false;
	bool _performingUndoRedo = false;
	bool _suppressHistoryRedoInvalidation = false;
	bool _revivedRetainedField = false;
	bool _fieldUndoAvailable = false;
	bool _fieldRedoAvailable = false;
	std::optional<TextWithTags> _fieldUndoNoopState;
	std::optional<TextWithTags> _fieldRedoNoopState;
	Markdown::MarkdownArticleSelection _selection;
	Markdown::MarkdownArticleSelectionEndpoints _selectionEndpoints;
	Markdown::PreparedEditSelection _structuralSelection;
	std::optional<BoundarySelectionOrigin> _boundarySelectionOrigin;
	Ui::VisibleRange _visibleRange;
	ArticleSelectionDrag _articleSelectionDrag;
	std::optional<Qt::Orientation> _horizontalScrollLock;
	bool _settingField = false;
	bool _trackingPointerPress = false;
	bool _inlineFieldExternalInteractionActive = false;
	Markdown::MarkdownArticleEditControlHit _pressedControl;
	std::optional<QPoint> _pressedControlPoint;
	HorizontalScrollDrag _horizontalScrollDrag = HorizontalScrollDrag::None;
	std::optional<QPoint> _pendingTouchHorizontalScrollPoint;
	bool _syncingInlineFieldGeometry = false;
	bool _pendingHeightOverrideUpdate = false;
};

} // namespace Iv::Editor
