// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once
#include <QString>
#include <QtNetwork/QNetworkReply>

#include "ayu/data/entities.h"
#include "core/application.h"

#include "data/data_session.h"
#include "history/history_item_components.h"

struct ApplyChanges
{
	std::vector<RegexFilter> newFilters;
	std::vector<QString> removeFiltersById;

	std::vector<RegexFilter> filtersOverrides;

	std::vector<RegexFilterGlobalExclusion> newExclusions;
	std::vector<RegexFilterGlobalExclusion> removeExclusions;

	std::map<long long, QString> peersToBeResolved;

	auto operator<=>(const ApplyChanges&) const = default;
};


class FilterUtils final: public QObject
{


	Q_OBJECT
public:
	static FilterUtils &getInstance() {
		static FilterUtils instance;
		return instance;
	}
	FilterUtils(const FilterUtils &) = delete;
	FilterUtils &operator=(const FilterUtils &) = delete;
	FilterUtils(FilterUtils &&) = delete;
	FilterUtils &operator=(FilterUtils &&) = delete;


	void importFromLink(const QString &link);
	bool importFromJson(const QByteArray &json);

	void publishFilters();
	static QString exportFilters();

	static QString extractAllText(not_null<HistoryItem *> item);
private:
	FilterUtils():
    _manager(std::make_unique<QNetworkAccessManager>()) {

	}

	bool handleResponse(const QByteArray &response);
	void gotFailure(const QNetworkReply::NetworkError &error);

	ApplyChanges prepareChanges(const QJsonObject &response);
	void applyChanges(const ApplyChanges &changes);

	QTimer* _timer = nullptr;

	std::unique_ptr<QNetworkAccessManager> _manager = nullptr;
	QNetworkReply *_reply = nullptr;
};

