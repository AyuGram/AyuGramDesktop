// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

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