#include "ayu_settings.h"

namespace AyuSettings {
    const QString filename = "tdata/ayu_settings.json";
    const int latestMigration = 1;
    std::optional<AyuGramSettings> settings = std::nullopt;

    AyuGramSettings &getInstance() {
        if (!settings.has_value()) {
            settings = std::optional(AyuGramSettings());
        }
        return settings.value();
    }

    void load() {
        QFile file(filename);
        if (!file.exists()) {
            return;
        }
        file.open(QIODevice::ReadOnly);
        QByteArray json = file.readAll();
        file.close();

        if (!settings.has_value()) {
            settings = std::optional(AyuGramSettings());
        }
        settings->fromJson(json);
    }

    void save() {
        if (!settings.has_value()) {
            settings = std::optional(AyuGramSettings());
        }

        settings->migrationVersion = latestMigration;
        QByteArray json = settings->toRawJson();

        QFile file(filename);
        file.open(QIODevice::WriteOnly);
        file.write(json);
        file.close();
    }
}
