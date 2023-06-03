#pragma once


#include <string>

// https://github.com/AyuGram/AyuGram4A/blob/main/TMessagesProj/src/main/java/com/radolyn/ayugram/database/entities/EditedMessage.java
class EditedMessage {
public:
    long userId;
    long dialogId;
    long messageId;

    std::string text;
    bool isDocument;
    std::string path;
    long date;
};
