// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "settings_ayu.h"
#include "ayu/ayu_settings.h"
#include "ayu/sync/ayu_sync_controller.h"
#include "ayu/ui/boxes/edit_deleted_mark.h"
#include "ayu/ui/boxes/edit_edited_mark.h"

#include "apiwrap.h"
#include "lang_auto.h"
#include "mainwindow.h"
#include "api/api_blocked_peers.h"
#include "boxes/connection_box.h"
#include "core/application.h"
#include "data/data_session.h"
#include "lang/lang_instance.h"
#include "main/main_session.h"
#include "platform/platform_specific.h"
#include "settings/settings_common.h"
#include "storage/localstorage.h"
#include "styles/style_basic.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include "ui/painter.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

constexpr auto GhostModeOptionsCount = 5;

class PainterHighQualityEnabler;

not_null<Ui::RpWidget*> AddInnerToggle(
	not_null<Ui::VerticalLayout*> container,
	const style::SettingsButton& st,
	std::vector<not_null<Ui::AbstractCheckView*>> innerCheckViews,
	not_null<Ui::SlideWrap<>*> wrap,
	rpl::producer<QString> buttonLabel,
	std::optional<QString> locked,
	Settings::IconDescriptor&& icon)
{
	const auto button = container->add(object_ptr<Ui::SettingsButton>(
		container,
		nullptr,
		st));
	if (icon)
	{
		AddButtonIcon(button, st, std::move(icon));
	}

	const auto toggleButton = Ui::CreateChild<Ui::SettingsButton>(
		container.get(),
		nullptr,
		st);

	struct State final
	{
		State(const style::Toggle& st, Fn<void()> c)
			: checkView(st, false, c)
		{
		}

		Ui::ToggleView checkView;
		Ui::Animations::Simple animation;
		rpl::event_stream<> anyChanges;
		std::vector<not_null<Ui::AbstractCheckView*>> innerChecks;
	};
	const auto state = button->lifetime().make_state<State>(
		st.toggle,
		[=] { toggleButton->update(); });
	state->innerChecks = std::move(innerCheckViews);
	const auto countChecked = [=]
	{
		return ranges::count_if(
			state->innerChecks,
			[](const auto& v) { return v->checked(); });
	};
	for (const auto& innerCheck : state->innerChecks)
	{
		innerCheck->checkedChanges(
		) | rpl::to_empty | start_to_stream(
			state->anyChanges,
			button->lifetime());
	}
	const auto checkView = &state->checkView;
	{
		const auto separator = Ui::CreateChild<Ui::RpWidget>(container.get());
		separator->paintRequest(
		) | start_with_next([=, bg = st.textBgOver]
		{
			auto p = QPainter(separator);
			p.fillRect(separator->rect(), bg);
		}, separator->lifetime());
		const auto separatorHeight = 2 * st.toggle.border
			+ st.toggle.diameter;
		button->geometryValue(
		) | start_with_next([=](const QRect& r)
		{
			const auto w = st::rightsButtonToggleWidth;
			constexpr auto kLineWidth = static_cast<int>(1);
			toggleButton->setGeometry(
				r.x() + r.width() - w,
				r.y(),
				w,
				r.height());
			separator->setGeometry(
				toggleButton->x() - kLineWidth,
				r.y() + (r.height() - separatorHeight) / 2,
				kLineWidth,
				separatorHeight);
		}, toggleButton->lifetime());

		const auto checkWidget = Ui::CreateChild<Ui::RpWidget>(toggleButton);
		checkWidget->resize(checkView->getSize());
		checkWidget->paintRequest(
		) | start_with_next([=]
		{
			auto p = QPainter(checkWidget);
			checkView->paint(p, 0, 0, checkWidget->width());
		}, checkWidget->lifetime());
		toggleButton->sizeValue(
		) | start_with_next([=](const QSize& s)
		{
			checkWidget->moveToRight(
				st.toggleSkip,
				(s.height() - checkWidget->height()) / 2);
		}, toggleButton->lifetime());
	}
	state->anyChanges.events_starting_with(
		rpl::empty_value()
	) | rpl::map(countChecked) | start_with_next([=](int count)
	{
		checkView->setChecked(count == GhostModeOptionsCount, anim::type::normal);
	}, toggleButton->lifetime());
	checkView->setLocked(locked.has_value());
	checkView->finishAnimating();

	const auto totalInnerChecks = state->innerChecks.size();
	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		button,
		combine(
			std::move(buttonLabel),
			state->anyChanges.events_starting_with(
				rpl::empty_value()
			) | rpl::map(countChecked)
		) | rpl::map([=](const QString& t, int checked)
		{
			auto count = Ui::Text::Bold("  "
				+ QString::number(checked)
				+ '/'
				+ QString::number(totalInnerChecks));
			return TextWithEntities::Simple(t).append(std::move(count));
		}));
	label->setAttribute(Qt::WA_TransparentForMouseEvents);
	const auto arrow = Ui::CreateChild<Ui::RpWidget>(button);
	{
		const auto& icon = st::permissionsExpandIcon;
		arrow->resize(icon.size());
		arrow->paintRequest(
		) | start_with_next([=, &icon]
		{
			auto p = QPainter(arrow);
			const auto center = QPointF(
				icon.width() / 2.,
				icon.height() / 2.);
			const auto progress = state->animation.value(
				wrap->toggled() ? 1. : 0.);
			auto hq = std::optional<PainterHighQualityEnabler>();
			if (progress > 0.)
			{
				hq.emplace(p);
				p.translate(center);
				p.rotate(progress * 180.);
				p.translate(-center);
			}
			icon.paint(p, 0, 0, arrow->width());
		}, arrow->lifetime());
	}
	button->sizeValue(
	) | start_with_next([=, &st](const QSize& s)
	{
		const auto labelLeft = st.padding.left();
		const auto labelRight = s.width() - toggleButton->width();

		label->resizeToWidth(labelRight - labelLeft - arrow->width());
		label->moveToLeft(
			labelLeft,
			(s.height() - label->height()) / 2);
		arrow->moveToLeft(
			std::min(
				labelLeft + label->naturalWidth(),
				labelRight - arrow->width()),
			(s.height() - arrow->height()) / 2);
	}, button->lifetime());
	wrap->toggledValue(
	) | rpl::skip(1) | start_with_next([=](bool toggled)
	{
		state->animation.start(
			[=] { arrow->update(); },
			toggled ? 0. : 1.,
			toggled ? 1. : 0.,
			st::slideWrapDuration);
	}, button->lifetime());

	const auto handleLocked = [=]
	{
		if (locked.has_value())
		{
			Ui::Toast::Show(container, *locked);
			return true;
		}
		return false;
	};

	button->clicks(
	) | start_with_next([=]
	{
		if (!handleLocked())
		{
			wrap->toggle(!wrap->toggled(), anim::type::normal);
		}
	}, button->lifetime());

	toggleButton->clicks(
	) | start_with_next([=]
	{
		if (!handleLocked())
		{
			const auto checked = !checkView->checked();
			for (const auto& innerCheck : state->innerChecks)
			{
				innerCheck->setChecked(checked, anim::type::normal);
			}
		}
	}, toggleButton->lifetime());

	return button;
}

namespace Settings
{
	rpl::producer<QString> Ayu::title()
	{
		return tr::ayu_AyuPreferences();
	}

	Ayu::Ayu(
		QWidget* parent,
		not_null<Window::SessionController*> controller)
		: Section(parent)
	{
		setupContent(controller);
	}

	void Ayu::SetupGhostEssentials(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_GhostEssentialsHeader());

		const auto widget = object_ptr<Ui::VerticalLayout>(this);

		widget->add(
			object_ptr<Ui::FlatLabel>(
				container,
				tr::ayu_GhostEssentialsHeader(),
				st::rightsHeaderLabel),
			st::rightsHeaderMargin);

		const auto addCheckbox = [&](
			not_null<Ui::VerticalLayout*> verticalLayout,
			const QString& label,
			const bool isCheckedOrig)
		{
			const auto checkView = [&]() -> not_null<Ui::AbstractCheckView*>
			{
				const auto checkbox = verticalLayout->add(
					object_ptr<Ui::Checkbox>(
						verticalLayout,
						label,
						isCheckedOrig,
						st::settingsCheckbox),
					st::rightsButton.padding);
				const auto button = Ui::CreateChild<Ui::RippleButton>(
					verticalLayout.get(),
					st::defaultRippleAnimation);
				button->stackUnder(checkbox);
				combine(
					verticalLayout->widthValue(),
					checkbox->geometryValue()
				) | start_with_next([=](int w, const QRect& r)
				{
					button->setGeometry(0, r.y(), w, r.height());
				}, button->lifetime());
				checkbox->setAttribute(Qt::WA_TransparentForMouseEvents);
				const auto checkView = checkbox->checkView();
				button->setClickedCallback([=]
				{
					checkView->setChecked(
						!checkView->checked(),
						anim::type::normal);
				});

				return checkView;
			}();
			checkView->checkedChanges(
			) | start_with_next([=](bool checked)
			{
			}, verticalLayout->lifetime());

			return checkView;
		};

		struct NestedEntry
		{
			QString checkboxLabel;
			bool initial;
			std::function<void(bool)> callback;
		};

		struct LabeledEntryGroup
		{
			std::vector<NestedEntry> nested;
		};

		std::vector checkboxes{
			NestedEntry{
				tr::ayu_DontReadMessages(tr::now), !settings->sendReadMessages, [=](bool enabled)
				{
					settings->set_sendReadMessages(!enabled);
					AyuSettings::save();
				}
			},
			NestedEntry{
				tr::ayu_DontReadStories(tr::now), !settings->sendReadStories, [=](bool enabled)
				{
					settings->set_sendReadStories(!enabled);
					AyuSettings::save();
				}
			},
			NestedEntry{
				tr::ayu_DontSendOnlinePackets(tr::now), !settings->sendOnlinePackets, [=](bool enabled)
				{
					settings->set_sendOnlinePackets(!enabled);
					AyuSettings::save();
				}
			},
			NestedEntry{
				tr::ayu_DontSendUploadProgress(tr::now), !settings->sendUploadProgress, [=](bool enabled)
				{
					settings->set_sendUploadProgress(!enabled);
					AyuSettings::save();
				}
			},
			NestedEntry{
				tr::ayu_SendOfflinePacketAfterOnline(tr::now), settings->sendOfflinePacketAfterOnline, [=](bool enabled)
				{
					settings->set_sendOfflinePacketAfterOnline(enabled);
					AyuSettings::save();
				}
			},
		};

		auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container));
		const auto verticalLayout = wrap->entity();
		auto innerChecks = std::vector<not_null<Ui::AbstractCheckView*>>();
		for (const auto& entry : checkboxes)
		{
			const auto c = addCheckbox(verticalLayout, entry.checkboxLabel, entry.initial);
			c->checkedValue(
			) | start_with_next([=](bool enabled)
			{
				entry.callback(enabled);
			}, container->lifetime());
			innerChecks.push_back(c);
		}

		const auto raw = wrap.data();
		raw->hide(anim::type::instant);
		AddInnerToggle(
			container,
			st::rightsButton,
			innerChecks,
			raw,
			tr::ayu_GhostModeToggle(),
			std::nullopt,
			{});
		container->add(std::move(wrap));
		container->widthValue(
		) | start_with_next([=](int w)
		{
			raw->resizeToWidth(w);
		}, raw->lifetime());

		AddButton(
			container,
			tr::ayu_MarkReadAfterSend(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->markReadAfterSend)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->markReadAfterSend);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_markReadAfterSend(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_UseScheduledMessages(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->useScheduledMessages)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->useScheduledMessages);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_useScheduledMessages(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupSpyEssentials(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_SpyEssentialsHeader());

		AddButton(
			container,
			tr::ayu_SaveDeletedMessages(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->saveDeletedMessages)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->saveDeletedMessages);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_keepDeletedMessages(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_SaveMessagesHistory(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->saveMessagesHistory)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->saveMessagesHistory);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_keepMessagesHistory(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupQoLToggles(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_QoLTogglesHeader());

		AddButton(
			container,
			tr::ayu_DisableAds(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->disableAds)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->disableAds);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_disableAds(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_LocalPremium(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->localPremium)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->localPremium);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_localPremium(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_DisableStories(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->disableStories)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->disableStories);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_disableStories(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_CopyUsernameAsLink(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->copyUsernameAsLink)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->copyUsernameAsLink);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_copyUsernameAsLink(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupCustomization(not_null<Ui::VerticalLayout*> container,
	                             not_null<Window::SessionController*> controller)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_CustomizationHeader());

		auto btn = AddButtonWithLabel(
			container,
			tr::ayu_DeletedMarkText(),
			AyuSettings::get_deletedMarkReactive(),
			st::settingsButtonNoIcon
		);
		btn->addClickHandler([=]()
		{
			auto box = Box<EditDeletedMarkBox>();
			Ui::show(std::move(box));
		});

		auto btn2 = AddButtonWithLabel(
			container,
			tr::ayu_EditedMarkText(),
			AyuSettings::get_editedMarkReactive(),
			st::settingsButtonNoIcon
		);
		btn2->addClickHandler([=]()
		{
			auto box = Box<EditEditedMarkBox>();
			Ui::show(std::move(box));
		});

		SetupRecentStickersLimitSlider(container);

		SetupShowPeerId(container, controller);

		AddButton(
			container,
			tr::ayu_HideAllChats(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->hideAllChatsFolder)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->hideAllChatsFolder);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_hideAllChatsFolder(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_ShowGhostToggleInDrawer(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->showGhostToggleInDrawer)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->showGhostToggleInDrawer);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_showGhostToggleInDrawer(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_SettingsShowMessageSeconds(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->showMessageSeconds)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->showMessageSeconds);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_showMessageSeconds(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupShowPeerId(not_null<Ui::VerticalLayout*> container,
	                          not_null<Window::SessionController*> controller)
	{
		auto settings = &AyuSettings::getInstance();

		const auto options = std::vector{
			QString(tr::ayu_SettingsShowID_Hide(tr::now)),
			QString("Telegram API"),
			QString("Bot API")
		};

		auto currentVal = AyuSettings::get_showPeerIdReactive() | rpl::map([=](int val)
		{
			return options[val];
		});

		const auto button = AddButtonWithLabel(
			container,
			tr::ayu_SettingsShowID(),
			currentVal,
			st::settingsButtonNoIcon);
		button->addClickHandler([=]
		{
			controller->show(Box([=](not_null<Ui::GenericBox*> box)
			{
				const auto save = [=](int index)
				{
					settings->set_showPeerId(index);
					AyuSettings::save();
				};
				SingleChoiceBox(box, {
					                .title = tr::ayu_SettingsShowID(),
					                .options = options,
					                .initialSelection = settings->showPeerId,
					                .callback = save,
				                });
			}));
		});
	}

	void Ayu::SetupRecentStickersLimitSlider(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		container->add(
			CreateButton(
				container,
				tr::ayu_SettingsRecentStickersCount(),
				st::settingsButtonNoIcon)
		);

		auto recentStickersLimitSlider = MakeSliderWithLabel(
			container,
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("300%"));
		container->add(
			std::move(recentStickersLimitSlider.widget),
			st::settingsScalePadding);
		const auto slider = recentStickersLimitSlider.slider;
		const auto label = recentStickersLimitSlider.label;

		const auto updateLabel = [=](int amount)
		{
			label->setText(QString::number(amount));
		};
		updateLabel(settings->recentStickersCount);

		slider->setPseudoDiscrete(
			100 + 1, // thx tg
			[=](int amount) { return amount; },
			settings->recentStickersCount,
			[=](int amount) { updateLabel(amount); },
			[=](int amount)
			{
				updateLabel(amount);

				settings->set_recentStickersCount(amount);
				AyuSettings::save();
			});
	}

	void Ayu::SetupAyuSync(not_null<Ui::VerticalLayout*> container)
	{
		AddSubsectionTitle(container, tr::ayu_AyuSyncHeader());

		auto text = AyuSync::isAgentDownloaded() ? tr::ayu_AyuSyncOpenPreferences() : tr::ayu_AyuSyncDownloadAgent();

		AddButton(
			container,
			text,
			st::settingsButtonNoIcon
		)->addClickHandler([=]
		{
			auto controller = &AyuSync::getInstance();
			controller->initializeAgent();
		});
	}

	void Ayu::SetupSendConfirmations(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_ConfirmationsTitle());

		AddButton(
			container,
			tr::ayu_StickerConfirmation(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->stickerConfirmation)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->stickerConfirmation);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_stickerConfirmation(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_GIFConfirmation(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->gifConfirmation)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->gifConfirmation);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_gifConfirmation(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_VoiceConfirmation(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->voiceConfirmation)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->voiceConfirmation);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_voiceConfirmation(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupAyuGramSettings(not_null<Ui::VerticalLayout*> container,
	                               not_null<Window::SessionController*> controller)
	{
		AddSkip(container);
		SetupGhostEssentials(container);
		AddSkip(container);

		AddDivider(container);

		AddSkip(container);
		SetupSpyEssentials(container);
		AddSkip(container);

		AddDivider(container);

		AddSkip(container);
		SetupQoLToggles(container);
		AddSkip(container);

		AddDivider(container);

		AddSkip(container);
		SetupCustomization(container, controller);
		AddSkip(container);
		AddDividerText(container, tr::ayu_SettingsCustomizationHint());

		// todo: compilation flag
		if (false)
		{
			AddSkip(container);
			SetupAyuSync(container);
			AddSkip(container);

			AddDivider(container);
		}

		AddSkip(container);
		SetupSendConfirmations(container);
		AddSkip(container);

		AddDividerText(container, tr::ayu_SettingsWatermark());
	}

	void Ayu::setupContent(not_null<Window::SessionController*> controller)
	{
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

		SetupAyuGramSettings(content, controller);

		ResizeFitChild(this, content);
	}
} // namespace Settings
