// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "entities.h"

namespace AyuDatabase {

void initialize();

void addEditedMessage(const EditedMessage &message);
std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit);
bool hasRevisions(ID userId, ID dialogId, ID messageId);

void addDeletedMessage(const DeletedMessage &message);
std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit);
bool hasDeletedMessages(ID userId, ID dialogId, ID topicId);

std::vector<RegexFilter> getAllRegexFilters();
RegexFilter getById(std::vector<char> id);
std::vector<RegexFilter> getShared();
std::vector<RegexFilter> getByDialogId(ID dialogId);
std::vector<RegexFilterGlobalExclusion> getAllFiltersExclusions();
std::vector<RegexFilter> getExcludedByDialogId(ID dialogId);

int getCount();


void addRegexFilter(const RegexFilter &filter);
void addRegexExclusion(const RegexFilterGlobalExclusion &exclusion);

void updateRegexFilter(const RegexFilter &filter);

void deleteFilter(const std::vector<char> &id);
void deleteExclusionsByFilterId(const std::vector<char> &id);
void deleteExclusion(ID dialogId, std::vector<char> filterId);

void deleteAllFilters();
void deleteAllExclusions();

bool hasFilters();
bool hasPerDialogFilters();

}
