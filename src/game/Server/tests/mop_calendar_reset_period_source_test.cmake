file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CalendarHandler.cpp" handler_source)

if(MUTATION STREQUAL "restore_subtraction")
    string(REPLACE
        "record.resetRemaining = int32(resetPeriod);"
        "record.resetRemaining = resetPeriod > currTime ? int32(resetPeriod - currTime) : 0;"
        handler_source "${handler_source}")
elseif(MUTATION STREQUAL "clamp_lockout_away")
    string(REPLACE
        "record.resetRemaining = state->GetResetTime() > currTime ?
                uint32(state->GetResetTime() - currTime) : 0;"
        "record.resetRemaining = uint32(state->GetResetTime());"
        handler_source "${handler_source}")
elseif(MUTATION STREQUAL "drop_period_source")
    string(REPLACE
        "GetMaxResetTimeFor(mapDiff)"
        "GetResetTimeFor(mapId, Difficulty(0))"
        handler_source "${handler_source}")
endif()

# Two adjacent loops fill two different countdown-looking fields, and only one
# of them is a countdown. Isolate them so a guard meant for one cannot be
# satisfied by the other.
string(FIND "${handler_source}"
    "MopCalendarPackets::CalendarListLockout record;" lockout_start)
string(FIND "${handler_source}"
    "std::vector<MopCalendarPackets::CalendarListReset> resetRecords;" reset_start)
string(FIND "${handler_source}"
    "WorldPacket data(SMSG_CALENDAR_SEND_CALENDAR);" send_start)
if(lockout_start EQUAL -1 OR reset_start EQUAL -1 OR send_start EQUAL -1
   OR NOT lockout_start LESS reset_start OR NOT reset_start LESS send_start)
    message(FATAL_ERROR
        "could not isolate the lockout and reset loops in HandleCalendarGetCalendar")
endif()
math(EXPR lockout_length "${reset_start} - ${lockout_start}")
math(EXPR reset_length "${send_start} - ${reset_start}")
string(SUBSTRING "${handler_source}" ${lockout_start} ${lockout_length} lockout_arm)
string(SUBSTRING "${handler_source}" ${reset_start} ${reset_length} reset_arm)

# A personal lockout carries DungeonPersistentState::GetResetTime(), a time_t
# absolute timestamp, so subtracting currTime is the correct countdown and the
# guard against going negative is required. This half must NOT be "fixed".
string(FIND "${lockout_arm}" "state->GetResetTime() > currTime" lockout_guard)
if(lockout_guard EQUAL -1)
    message(FATAL_ERROR
        "the personal lockout countdown must keep its guard: GetResetTime() is "
        "an absolute time_t, so currTime can legitimately exceed it")
endif()

# The global reset word is not a countdown at all. GetMaxResetTimeFor returns a
# DURATION - RaidDuration scaled and floored to whole days, minimum one day -
# and the client uses it as a recurrence period, stepping from the calendar
# epoch by this value and dividing by it. Retail carries 604800, 259200 and
# 86400 and never zero. Comparing a duration against an absolute currTime can
# only ever be false, which is how every record came to be sent as zero.
string(FIND "${reset_arm}" "GetMaxResetTimeFor(mapDiff)" period_source)
if(period_source EQUAL -1)
    message(FATAL_ERROR
        "the global reset word must come from GetMaxResetTimeFor, the helper "
        "that yields the retail period class")
endif()

string(FIND "${reset_arm}" "record.resetRemaining = int32(resetPeriod);" direct_assign)
if(direct_assign EQUAL -1)
    message(FATAL_ERROR
        "the global reset period must be sent as-is, not turned into a countdown")
endif()

# Match the operator forms, not a bare "currTime": the explanatory comment in
# the source names the variable, and a substring check would trip on prose.
string(FIND "${reset_arm}" "> currTime" reset_compares)
string(FIND "${reset_arm}" "- currTime" reset_subtracts)
if(NOT reset_compares EQUAL -1 OR NOT reset_subtracts EQUAL -1)
    message(FATAL_ERROR
        "the global reset arm must not compare against or subtract currTime: "
        "the value is a period, and measuring a duration against an absolute "
        "time is the defect that zeroed every record")
endif()
