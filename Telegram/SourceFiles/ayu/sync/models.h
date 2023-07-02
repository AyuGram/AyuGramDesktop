#pragma once

#include <string>
#include <vector>

class SyncEvent {
    std::string type = "sync_unspecified";
    long userId = 0;
};

class SyncBatch : public SyncEvent {
    std::string type = "sync_batch";
    long userId;

    class SyncBatchArgs {
        std::vector<SyncEvent> events;
    };

    SyncBatchArgs args;
};

class SyncRead : public SyncEvent {
    std::string type = "sync_read";
    long userId;

    class SyncReadArgs {
        long dialogId;
        int untilId;
        int unread;
    };

    SyncReadArgs args;
};

class SyncForce : public SyncEvent {
    std::string type = "sync_force";
    long userId;

    class SyncForceArgs {
        int fromDate;
    };

    SyncForceArgs args;
};

class SyncForceFinish : public SyncEvent {
    std::string type = "sync_force_finish";
    long userId;

    class SyncForceFinishArgs {
    };

    SyncForceFinishArgs args;
};

