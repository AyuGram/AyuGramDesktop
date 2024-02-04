// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include "ayu/data/entities.h"
#include "main/main_session.h"

#include "base/unique_qptr.h"

namespace Ui {
namespace Menu {
class Menu;
class ItemBase;
} // namespace Menu

class PopupMenu;

[[nodiscard]] base::unique_qptr<Menu::ItemBase> ContextActionWithSubText(
	not_null<Menu::Menu*> menu,
	const style::icon &icon,
	const QString &title,
	const QString &subtext,
	Fn<void()> callback = nullptr);

[[nodiscard]] base::unique_qptr<Menu::ItemBase> ContextActionStickerAuthor(
	not_null<Menu::Menu*> menu,
	not_null<Main::Session*> session,
	ID authorId);

} // namespace Ui
