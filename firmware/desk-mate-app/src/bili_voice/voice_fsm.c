#include "voice_contracts.h"

#include <stdio.h>
#include <string.h>

static int64_t ms_to_us(int64_t ms) { return ms * 1000; }

static void emit_action(voice_fsm_t *fsm, voice_action_type_t type,
                        voice_cancel_reason_t reason, const char *summary) {
    if (!fsm->io.emit_action) return;
    voice_action_t action;
    memset(&action, 0, sizeof(action));
    action.type = type;
    action.reason = reason;
    snprintf(action.command_id, sizeof(action.command_id), "%s", fsm->command_id);
    snprintf(action.replay_of, sizeof(action.replay_of), "%s", fsm->last_replay_id);
    snprintf(action.sha256, sizeof(action.sha256), "%s", fsm->sha256);
    snprintf(action.summary, sizeof(action.summary), "%s", summary ? summary : "");
    fsm->io.emit_action(fsm->io.ctx, &action);
}

static void emit_audio(voice_fsm_t *fsm, bool start) {
    if (fsm->io.emit_audio) fsm->io.emit_audio(fsm->io.ctx, start);
}

static void new_command(voice_fsm_t *fsm) {
    memset(fsm->command_id, 0, sizeof(fsm->command_id));
    if (fsm->io.uuid4) fsm->io.uuid4(fsm->io.ctx, fsm->command_id);
    fsm->begin_sent = false;
    fsm->begin_ack = false;
    fsm->seal_sent = false;
    fsm->seal_ack = false;
    fsm->commit_sent = false;
    fsm->cancel_sent = false;
    fsm->sha256[0] = 0;
}

static void set_state(voice_fsm_t *fsm, voice_state_t state, int64_t now_us) {
    fsm->state = state;
    fsm->state_since_us = now_us;
}

static void start_arm(voice_fsm_t *fsm, int64_t now_us, int64_t raw_us) {
    fsm->down_us = raw_us;
    fsm->release_us = 0;
    fsm->commit_at_us = 0;
    fsm->cancel_deadline_us = 0;
    fsm->qualified = false;
    fsm->release_before_qualify = false;
    fsm->short_press_pending = false;
    fsm->waiting_release = false;
    fsm->seal_requested = false;
    fsm->audio_active = true;
    fsm->result = VOICE_RESULT_NONE;
    fsm->code = 0;
    fsm->summary[0] = 0;
    new_command(fsm);
    set_state(fsm, VOICE_STATE_ARMING, now_us);
    emit_action(fsm, VOICE_ACT_AUDIO_RESET, VOICE_REASON_USER, "reset");
    emit_audio(fsm, true);
}

static void stop_capture(voice_fsm_t *fsm, int64_t now_us) {
    (void)now_us;
    if (fsm->audio_active) {
        fsm->audio_active = false;
        emit_audio(fsm, false);
    }
}

static void set_cancel_deadlines(voice_fsm_t *fsm, int64_t now_us) {
    int64_t base = fsm->release_us;
    fsm->cancel_deadline_us = base + ms_to_us(VOICE_CANCEL_WINDOW_MS + VOICE_INPUT_UNCERTAINTY_MS);
    fsm->commit_at_us = base + ms_to_us(VOICE_COMMIT_EARLIEST_MS);
    (void)now_us;
}

static void request_seal(voice_fsm_t *fsm, int64_t now_us) {
    fsm->seal_requested = true;
    if (fsm->begin_ack && !fsm->seal_sent) {
        fsm->seal_sent = true;
        emit_action(fsm, VOICE_ACT_SEAL, VOICE_REASON_USER, "seal");
    }
    (void)now_us;
}

static void begin_cancel(voice_fsm_t *fsm, voice_cancel_reason_t reason,
                         int64_t now_us, bool local_only) {
    stop_capture(fsm, now_us);
    if (!local_only && fsm->begin_sent) {
        emit_action(fsm, VOICE_ACT_CANCEL, reason, "cancel");
        fsm->cancel_sent = true;
    }
    fsm->waiting_release = fsm->a_down;
    if (fsm->waiting_release) set_state(fsm, VOICE_STATE_WAIT_RELEASE, now_us);
}

static void finish_result(voice_fsm_t *fsm, voice_result_t result, int code,
                          const char *summary, int64_t now_us) {
    stop_capture(fsm, now_us);
    fsm->result = result;
    fsm->code = code;
    snprintf(fsm->summary, sizeof(fsm->summary), "%s", summary ? summary : "");
    fsm->begin_sent = false;
    fsm->seal_requested = false;
    fsm->commit_sent = false;
    if (fsm->waiting_release) {
        fsm->unknown_ack_required = (result == VOICE_RESULT_UNKNOWN);
        set_state(fsm, VOICE_STATE_WAIT_RELEASE, now_us);
        return;
    }
    if (result == VOICE_RESULT_UNKNOWN) {
        fsm->unknown_ack_required = true;
        set_state(fsm, VOICE_STATE_OUTCOME_UNKNOWN_ACK, now_us);
    } else {
        fsm->unknown_ack_required = false;
        set_state(fsm, VOICE_STATE_RESULT, now_us);
    }
}

static bool is_capturing_state(voice_state_t state) {
    return state == VOICE_STATE_ARMING || state == VOICE_STATE_RECORDING ||
           state == VOICE_STATE_CANCEL_WINDOW;
}

void voice_fsm_init(voice_fsm_t *fsm, const voice_fsm_io_t *io) {
    memset(fsm, 0, sizeof(*fsm));
    fsm->io = *io;
    fsm->state = VOICE_STATE_OFFLINE;
    fsm->result = VOICE_RESULT_NONE;
    fsm->state_since_us = 0;
}

void voice_fsm_handle(voice_fsm_t *fsm, const voice_event_t *event, int64_t now_us) {
    if (!fsm || !event) return;
    switch (event->type) {
    case VOICE_EV_A_DOWN:
        fsm->a_down = true;
        if (fsm->state == VOICE_STATE_READY) {
            start_arm(fsm, now_us, event->raw_us);
        } else if (fsm->state == VOICE_STATE_RESULT) {
            /* The feedback overlay can be dismissed with KEY immediately;
             * the same key press starts the next PTT. */
            fsm->result = VOICE_RESULT_NONE;
            fsm->code = 0;
            fsm->summary[0] = 0;
            start_arm(fsm, now_us, event->raw_us);
        } else if (fsm->state == VOICE_STATE_CANCEL_WINDOW) {
            if (fsm->cancel_deadline_us == 0 || event->raw_us <= fsm->cancel_deadline_us) {
                begin_cancel(fsm, VOICE_REASON_USER, now_us, false);
                finish_result(fsm, VOICE_RESULT_CANCELED, 0, "canceled", now_us);
            }
        } else if (fsm->state == VOICE_STATE_REPLAY_ARMED ||
                   fsm->state == VOICE_STATE_REPLAY_PREPARE) {
            begin_cancel(fsm, VOICE_REASON_USER, now_us, true);
            finish_result(fsm, VOICE_RESULT_CANCELED, 0, "canceled", now_us);
        } else if (fsm->state == VOICE_STATE_OUTCOME_UNKNOWN_ACK) {
            /* KEY acknowledges an UNKNOWN result and immediately resumes PTT. */
            fsm->unknown_ack_required = false;
            fsm->result = VOICE_RESULT_NONE;
            fsm->code = 0;
            fsm->summary[0] = 0;
            set_state(fsm, VOICE_STATE_READY, now_us);
        }
        break;
    case VOICE_EV_A_UP:
        fsm->a_down = false;
        if (fsm->state == VOICE_STATE_ARMING) {
            int64_t hold_us = event->raw_us - fsm->down_us;
            if (hold_us < ms_to_us(VOICE_MIN_HOLD_MS)) {
                stop_capture(fsm, now_us);
                emit_action(fsm, VOICE_ACT_AUDIO_RESET, VOICE_REASON_SHORT_PRESS, "short");
                fsm->result = VOICE_RESULT_NONE;
                set_state(fsm, VOICE_STATE_READY, now_us);
            } else if (hold_us < ms_to_us(VOICE_QUALIFY_MS)) {
                fsm->release_before_qualify = true;
                fsm->release_us = event->raw_us;
                stop_capture(fsm, now_us);
            } else {
                fsm->release_us = event->raw_us;
                stop_capture(fsm, now_us);
                set_state(fsm, VOICE_STATE_CANCEL_WINDOW, now_us);
                set_cancel_deadlines(fsm, now_us);
                request_seal(fsm, now_us);
            }
        } else if (fsm->state == VOICE_STATE_RECORDING) {
            fsm->release_us = event->raw_us;
            stop_capture(fsm, now_us);
            set_state(fsm, VOICE_STATE_CANCEL_WINDOW, now_us);
            set_cancel_deadlines(fsm, now_us);
            request_seal(fsm, now_us);
        } else if (fsm->state == VOICE_STATE_WAIT_RELEASE) {
            fsm->waiting_release = false;
            if (fsm->unknown_ack_required) {
                set_state(fsm, VOICE_STATE_OUTCOME_UNKNOWN_ACK, now_us);
            } else {
                set_state(fsm, VOICE_STATE_READY, now_us);
                fsm->result = VOICE_RESULT_NONE;
                fsm->summary[0] = 0;
            }
        }
        break;
    case VOICE_EV_B_DOWN:
        if (fsm->state == VOICE_STATE_READY && fsm->last_replayable && fsm->last_replay_id[0]) {
            new_command(fsm);
            fsm->replay_prepared = false;
            set_state(fsm, VOICE_STATE_REPLAY_PREPARE, now_us);
            emit_action(fsm, VOICE_ACT_REPLAY_PREPARE, VOICE_REASON_USER, "replay");
        } else if (fsm->state == VOICE_STATE_REPLAY_ARMED) {
            fsm->commit_sent = true;
            set_state(fsm, VOICE_STATE_COMMITTING, now_us);
            emit_action(fsm, VOICE_ACT_REPLAY_COMMIT, VOICE_REASON_USER, "replay commit");
        } else if (fsm->state == VOICE_STATE_OUTCOME_UNKNOWN_ACK) {
            fsm->unknown_ack_required = false;
            fsm->result = VOICE_RESULT_NONE;
            fsm->code = 0;
            fsm->summary[0] = 0;
            set_state(fsm, VOICE_STATE_READY, now_us);
        }
        break;
    case VOICE_EV_C_SHORT:
        if (is_capturing_state(fsm->state)) {
            begin_cancel(fsm, VOICE_REASON_PAGE_EXIT, now_us, false);
            finish_result(fsm, VOICE_RESULT_CANCELED, 0, "canceled", now_us);
        } else if (fsm->state == VOICE_STATE_COMMITTING ||
                   fsm->state == VOICE_STATE_WAITING ||
                   fsm->state == VOICE_STATE_QUERYING) {
            emit_action(fsm, VOICE_ACT_CANCEL, VOICE_REASON_PAGE_EXIT, "cancel");
            fsm->cancel_sent = true;
            fsm->waiting_release = fsm->a_down;
            if (fsm->waiting_release) set_state(fsm, VOICE_STATE_WAIT_RELEASE, now_us);
        } else if (fsm->state == VOICE_STATE_REPLAY_ARMED ||
                   fsm->state == VOICE_STATE_REPLAY_PREPARE) {
            begin_cancel(fsm, VOICE_REASON_PAGE_EXIT, now_us, true);
            finish_result(fsm, VOICE_RESULT_CANCELED, 0, "canceled", now_us);
        }
        break;
    case VOICE_EV_TRANSPORT_READY:
        fsm->connected = true;
        if (fsm->state == VOICE_STATE_OFFLINE || fsm->state == VOICE_STATE_CONNECTING) {
            set_state(fsm, VOICE_STATE_READY, now_us);
        } else if ((fsm->state == VOICE_STATE_QUERYING || fsm->state == VOICE_STATE_COMMITTING ||
                    fsm->state == VOICE_STATE_WAITING) && fsm->commit_sent) {
            emit_action(fsm, VOICE_ACT_STATUS, VOICE_REASON_NETWORK_ERROR, "status");
            set_state(fsm, VOICE_STATE_QUERYING, now_us);
        }
        break;
    case VOICE_EV_TRANSPORT_LOST:
        fsm->connected = false;
        if (is_capturing_state(fsm->state) || fsm->state == VOICE_STATE_COMMITTING) {
            begin_cancel(fsm, VOICE_REASON_NETWORK_ERROR, now_us, true);
            finish_result(fsm, VOICE_RESULT_FAILED, 0, "network lost", now_us);
        } else if (fsm->state == VOICE_STATE_WAITING || fsm->state == VOICE_STATE_QUERYING) {
            set_state(fsm, VOICE_STATE_QUERYING, now_us);
        } else {
            set_state(fsm, VOICE_STATE_OFFLINE, now_us);
        }
        break;
    case VOICE_EV_BEGIN_ACK:
        fsm->begin_ack = true;
        if (fsm->seal_requested && !fsm->seal_sent) {
            fsm->seal_sent = true;
            emit_action(fsm, VOICE_ACT_SEAL, VOICE_REASON_USER, "seal");
        }
        break;
    case VOICE_EV_SEAL_ACK:
        fsm->seal_ack = true;
        snprintf(fsm->sha256, sizeof(fsm->sha256), "%s", event->sha256);
        if (fsm->state == VOICE_STATE_CANCEL_WINDOW && fsm->commit_at_us > 0 &&
            now_us >= fsm->commit_at_us && !fsm->commit_sent) {
            fsm->commit_sent = true;
            set_state(fsm, VOICE_STATE_COMMITTING, now_us);
            emit_action(fsm, VOICE_ACT_COMMIT, VOICE_REASON_USER, "commit");
        }
        break;
    case VOICE_EV_COMMIT_ACK:
        if (fsm->state == VOICE_STATE_COMMITTING) {
            set_state(fsm, VOICE_STATE_WAITING, now_us);
            snprintf(fsm->summary, sizeof(fsm->summary), "%s", "processing");
        }
        break;
    case VOICE_EV_CANCEL_ACK:
        finish_result(fsm, VOICE_RESULT_CANCELED, 0, "canceled", now_us);
        break;
    case VOICE_EV_REPLAY_READY:
        if (fsm->state == VOICE_STATE_REPLAY_PREPARE) {
            fsm->replay_prepared = true;
            snprintf(fsm->sha256, sizeof(fsm->sha256), "%s", event->sha256);
            set_state(fsm, VOICE_STATE_REPLAY_ARMED, now_us);
            snprintf(fsm->summary, sizeof(fsm->summary), "%s", "again for replay");
        }
        break;
    case VOICE_EV_RESULT:
        if (event->result == VOICE_RESULT_SUCCESS && event->replayable && event->command_id[0]) {
            fsm->last_replayable = true;
            snprintf(fsm->last_replay_id, sizeof(fsm->last_replay_id), "%s", event->command_id);
        } else if (event->result != VOICE_RESULT_CANCELED) {
            fsm->last_replayable = false;
        }
        finish_result(fsm, event->result, event->code, event->summary, now_us);
        break;
    case VOICE_EV_ERROR:
        finish_result(fsm, VOICE_RESULT_FAILED, event->code, event->summary, now_us);
        break;
    default:
        break;
    }
}

void voice_fsm_tick(voice_fsm_t *fsm, int64_t now_us) {
    if (!fsm) return;
    switch (fsm->state) {
    case VOICE_STATE_ARMING:
        if (now_us - fsm->down_us >= ms_to_us(VOICE_QUALIFY_MS)) {
            fsm->qualified = true;
            if (!fsm->begin_sent) {
                fsm->begin_sent = true;
                emit_action(fsm, VOICE_ACT_BEGIN, VOICE_REASON_USER, "begin");
            }
            if (fsm->release_before_qualify) {
                set_state(fsm, VOICE_STATE_CANCEL_WINDOW, now_us);
                set_cancel_deadlines(fsm, now_us);
                request_seal(fsm, now_us);
            } else {
                set_state(fsm, VOICE_STATE_RECORDING, now_us);
            }
        }
        break;
    case VOICE_STATE_RECORDING:
        if (now_us - fsm->down_us >= ms_to_us(VOICE_MAX_CAPTURE_MS)) {
            begin_cancel(fsm, VOICE_REASON_TIMEOUT, now_us, false);
            finish_result(fsm, VOICE_RESULT_FAILED, 0, "recording timeout", now_us);
        }
        break;
    case VOICE_STATE_CANCEL_WINDOW:
        if (now_us - fsm->down_us >= ms_to_us(VOICE_MAX_CAPTURE_MS)) {
            begin_cancel(fsm, VOICE_REASON_TIMEOUT, now_us, false);
            finish_result(fsm, VOICE_RESULT_FAILED, 0, "recording timeout", now_us);
            break;
        }
        if (fsm->commit_at_us > 0 && now_us >= fsm->commit_at_us) {
            if (fsm->begin_ack && fsm->seal_ack && !fsm->commit_sent) {
                fsm->commit_sent = true;
                set_state(fsm, VOICE_STATE_COMMITTING, now_us);
                emit_action(fsm, VOICE_ACT_COMMIT, VOICE_REASON_USER, "commit");
            } else if (now_us >= fsm->commit_at_us + ms_to_us(VOICE_SEAL_TIMEOUT_MS)) {
                begin_cancel(fsm, VOICE_REASON_AUDIO_ERROR, now_us, false);
                finish_result(fsm, VOICE_RESULT_FAILED, 0, "seal timeout", now_us);
            }
        }
        break;
    case VOICE_STATE_COMMITTING:
        if (now_us - fsm->state_since_us >= ms_to_us(VOICE_COMMIT_ACK_MS)) {
            emit_action(fsm, VOICE_ACT_STATUS, VOICE_REASON_NETWORK_ERROR, "status");
            set_state(fsm, VOICE_STATE_QUERYING, now_us);
        }
        break;
    case VOICE_STATE_QUERYING:
        if (now_us - fsm->state_since_us >= ms_to_us(VOICE_STATUS_TOTAL_MS)) {
            finish_result(fsm, VOICE_RESULT_UNKNOWN, 0, "outcome unknown", now_us);
        }
        break;
    case VOICE_STATE_REPLAY_PREPARE:
        if (now_us - fsm->state_since_us >= ms_to_us(10000)) {
            begin_cancel(fsm, VOICE_REASON_USER, now_us, true);
            finish_result(fsm, VOICE_RESULT_FAILED, 0, "replay timeout", now_us);
        }
        break;
    case VOICE_STATE_REPLAY_ARMED:
        if (now_us - fsm->state_since_us >= ms_to_us(10000)) {
            begin_cancel(fsm, VOICE_REASON_USER, now_us, true);
            finish_result(fsm, VOICE_RESULT_FAILED, 0, "replay timeout", now_us);
        }
        break;
    case VOICE_STATE_RESULT:
        if (now_us - fsm->state_since_us >= ms_to_us(VOICE_RESULT_HOLD_MS)) {
            set_state(fsm, VOICE_STATE_READY, now_us);
            fsm->result = VOICE_RESULT_NONE;
            fsm->summary[0] = 0;
        }
        break;
    case VOICE_STATE_OUTCOME_UNKNOWN_ACK:
        // Never trap the user in UNKNOWN; KEY/B or this timeout returns to PTT.
        if (now_us - fsm->state_since_us >= ms_to_us(10000)) {
            fsm->unknown_ack_required = false;
            fsm->result = VOICE_RESULT_NONE;
            fsm->code = 0;
            fsm->summary[0] = 0;
            set_state(fsm, VOICE_STATE_READY, now_us);
        }
        break;
    default:
        break;
    }
}

bool voice_fsm_is_capturing(const voice_fsm_t *fsm) {
    return fsm && (fsm->audio_active || is_capturing_state(fsm->state));
}

bool voice_fsm_should_commit(const voice_fsm_t *fsm) {
    return fsm && fsm->state == VOICE_STATE_CANCEL_WINDOW &&
           fsm->commit_at_us > 0 && fsm->begin_ack && fsm->seal_ack && !fsm->commit_sent;
}

void voice_fsm_uuid4_format(uint8_t bytes[16], char out[37]) {
    bytes[6] = (uint8_t)((bytes[6] & 0x0F) | 0x40);
    bytes[8] = (uint8_t)((bytes[8] & 0x3F) | 0x80);
    snprintf(out, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
             bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
}
