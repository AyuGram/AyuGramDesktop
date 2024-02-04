// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include <QString>
#include "json.hpp"

inline void to_json(nlohmann::json &j, const QString &q) {
	j = nlohmann::json(q.toStdString());
}

inline void from_json(const nlohmann::json &j, QString &q) {
	q = QString::fromStdString(j.get<std::string>());
}
