#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../desk-mate-app/src/bili_voice/voice_contracts.h"

typedef struct {
    int actions[64];
    int nactions;
    int audio_starts;
    int audio_stops;
    char command_id[37];
    int uuid_counter;
} mock_t;

static void mock_emit(void *ctx, const voice_action_t *action) {
    mock_t *m = (mock_t *)ctx;
    assert(m->nactions < 64);
    m->actions[m->nactions++] = (int)action->type;
}
static void mock_audio(void *ctx, bool start) {
    mock_t *m = (mock_t *)ctx;
    if (start) m->audio_starts++; else m->audio_stops++;
}
static void mock_uuid(void *ctx, char out[37]) {
    mock_t *m = (mock_t *)ctx;
    m->uuid_counter++;
    snprintf(out, 37, "00000000-0000-4000-8000-%012d", m->uuid_counter);
    snprintf(m->command_id, sizeof(m->command_id), "%s", out);
}

static voice_fsm_t fsm;
static mock_t mock;

static void setup(void) {
    memset(&mock, 0, sizeof(mock));
    voice_fsm_io_t io = {mock_emit, mock_audio, mock_uuid, &mock};
    voice_fsm_init(&fsm, &io);
    voice_event_t ready;
    memset(&ready, 0, sizeof(ready));
    ready.type = VOICE_EV_TRANSPORT_READY;
    voice_fsm_handle(&fsm, &ready, 0);
    assert(fsm.state == VOICE_STATE_READY);
}

static void event(voice_event_type_t type, int64_t raw_us) {
    voice_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.raw_us = raw_us;
    ev.stable_us = raw_us;
    ev.source = VOICE_INPUT_KEY10;
    voice_fsm_handle(&fsm, &ev, raw_us);
}

static bool has_action(int type) {
    for (int i = 0; i < mock.nactions; ++i) if (mock.actions[i] == type) return true;
    return false;
}

static void complete_to_recording(int64_t down) {
    event(VOICE_EV_A_DOWN, down);
    assert(fsm.state == VOICE_STATE_ARMING);
    voice_fsm_tick(&fsm, down + 530000);
    assert(fsm.state == VOICE_STATE_RECORDING);
    assert(has_action(VOICE_ACT_BEGIN));
    voice_event_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = VOICE_EV_BEGIN_ACK;
    voice_fsm_handle(&fsm, &ack, down + 540000);
    assert(fsm.begin_ack);
}

static void valid_flow_test(void) {
    setup();
    int64_t down = 1000000;
    complete_to_recording(down);
    int64_t release = down + 1000000;
    event(VOICE_EV_A_UP, release);
    assert(fsm.state == VOICE_STATE_CANCEL_WINDOW);
    assert(has_action(VOICE_ACT_SEAL));
    voice_event_t seal;
    memset(&seal, 0, sizeof(seal));
    seal.type = VOICE_EV_SEAL_ACK;
    memset(seal.sha256, '0', 64);
    voice_fsm_handle(&fsm, &seal, release + 100000);
    voice_fsm_tick(&fsm, release + 839000);
    assert(fsm.state == VOICE_STATE_CANCEL_WINDOW);
    voice_fsm_tick(&fsm, release + 840000);
    assert(fsm.state == VOICE_STATE_COMMITTING);
    assert(has_action(VOICE_ACT_COMMIT));
    voice_event_t commit_ack;
    memset(&commit_ack, 0, sizeof(commit_ack));
    commit_ack.type = VOICE_EV_COMMIT_ACK;
    voice_fsm_handle(&fsm, &commit_ack, release + 900000);
    assert(fsm.state == VOICE_STATE_WAITING);
    voice_event_t result;
    memset(&result, 0, sizeof(result));
    result.type = VOICE_EV_RESULT;
    result.result = VOICE_RESULT_SUCCESS;
    result.replayable = true;
    snprintf(result.command_id, sizeof(result.command_id), "%s", fsm.command_id);
    snprintf(result.summary, sizeof(result.summary), "done");
    voice_fsm_handle(&fsm, &result, release + 1000000);
    assert(fsm.state == VOICE_STATE_RESULT);
    assert(fsm.last_replayable);
    voice_fsm_tick(&fsm, release + 3000000);
    assert(fsm.state == VOICE_STATE_READY);
    assert(mock.audio_starts == 1 && mock.audio_stops == 1);
    puts("fsm: valid hold, begin/seal/commit, result PASS");
}

static void short_hold_test(void) {
    setup();
    int64_t down = 2000000;
    event(VOICE_EV_A_DOWN, down);
    event(VOICE_EV_A_UP, down + 499000);
    assert(fsm.state == VOICE_STATE_READY);
    assert(!has_action(VOICE_ACT_BEGIN));
    assert(!has_action(VOICE_ACT_SEAL));
    assert(mock.audio_stops == 1);
    puts("fsm: <500ms short press no upload PASS");
}

static void hold_500_to_530_test(void) {
    setup();
    int64_t down = 3000000;
    event(VOICE_EV_A_DOWN, down);
    event(VOICE_EV_A_UP, down + 500000);
    assert(fsm.state == VOICE_STATE_ARMING);
    voice_fsm_tick(&fsm, down + 529000);
    assert(!has_action(VOICE_ACT_BEGIN));
    voice_fsm_tick(&fsm, down + 530000);
    assert(fsm.state == VOICE_STATE_CANCEL_WINDOW);
    assert(has_action(VOICE_ACT_BEGIN));
    assert(mock.audio_stops == 1);
    puts("fsm: 500-530ms qualifies at fence and seals PASS");
}

static void cancel_800_test(int64_t offset_ms) {
    setup();
    int64_t down = 4000000;
    complete_to_recording(down);
    int64_t release = down + 1000000;
    event(VOICE_EV_A_UP, release);
    voice_event_t seal;
    memset(&seal, 0, sizeof(seal));
    seal.type = VOICE_EV_SEAL_ACK;
    memset(seal.sha256, '0', 64);
    voice_fsm_handle(&fsm, &seal, release + 100000);
    event(VOICE_EV_A_DOWN, release + offset_ms * 1000);
    if (offset_ms <= 810) {
        assert(fsm.state == VOICE_STATE_WAIT_RELEASE);
        assert(has_action(VOICE_ACT_CANCEL));
        event(VOICE_EV_A_UP, release + offset_ms * 1000 + 50000);
        assert(fsm.state == VOICE_STATE_READY);
    } else {
        assert(fsm.state == VOICE_STATE_CANCEL_WINDOW);
        event(VOICE_EV_A_UP, release + offset_ms * 1000 + 50000);
        voice_fsm_tick(&fsm, release + 840000);
        assert(fsm.state == VOICE_STATE_COMMITTING);
    }
}

static void cancel_tests(void) {
    cancel_800_test(799);
    cancel_800_test(800);
    cancel_800_test(801);
    cancel_800_test(811);
    puts("fsm: 799/800/801 cancel, 811 outside window PASS");
}

static void timeout_test(void) {
    setup();
    int64_t down = 5000000;
    complete_to_recording(down);
    voice_fsm_tick(&fsm, down + 60000000);
    assert(fsm.state == VOICE_STATE_WAIT_RELEASE);
    assert(has_action(VOICE_ACT_CANCEL));
    event(VOICE_EV_A_UP, down + 60500000);
    assert(fsm.state == VOICE_STATE_READY);
    puts("fsm: 60s recording timeout aborts without commit PASS");
}

static void replay_test(void) {
    setup();
    int64_t down = 6000000;
    complete_to_recording(down);
    int64_t release = down + 1000000;
    event(VOICE_EV_A_UP, release);
    voice_event_t seal;
    memset(&seal, 0, sizeof(seal));
    seal.type = VOICE_EV_SEAL_ACK;
    memset(seal.sha256, '0', 64);
    voice_fsm_handle(&fsm, &seal, release + 100000);
    voice_fsm_tick(&fsm, release + 840000);
    voice_event_t commit_ack;
    memset(&commit_ack, 0, sizeof(commit_ack));
    commit_ack.type = VOICE_EV_COMMIT_ACK;
    voice_fsm_handle(&fsm, &commit_ack, release + 900000);
    voice_event_t result;
    memset(&result, 0, sizeof(result));
    result.type = VOICE_EV_RESULT;
    result.result = VOICE_RESULT_SUCCESS;
    result.replayable = true;
    snprintf(result.command_id, sizeof(result.command_id), "%s", fsm.command_id);
    voice_fsm_handle(&fsm, &result, release + 1000000);
    voice_fsm_tick(&fsm, release + 3000000);
    assert(fsm.state == VOICE_STATE_READY);
    assert(fsm.last_replayable);
    mock.nactions = 0;
    event(VOICE_EV_B_DOWN, release + 4000000);
    assert(fsm.state == VOICE_STATE_REPLAY_PREPARE);
    assert(has_action(VOICE_ACT_REPLAY_PREPARE));
    voice_event_t ready;
    memset(&ready, 0, sizeof(ready));
    ready.type = VOICE_EV_REPLAY_READY;
    memset(ready.sha256, '1', 64);
    voice_fsm_handle(&fsm, &ready, release + 4100000);
    assert(fsm.state == VOICE_STATE_REPLAY_ARMED);
    event(VOICE_EV_B_DOWN, release + 4200000);
    assert(fsm.state == VOICE_STATE_COMMITTING);
    assert(has_action(VOICE_ACT_REPLAY_COMMIT));
    puts("fsm: B replay prepare then new B commit PASS");
}

static void unknown_ack_test(void);
static void result_key_rearm_test(void);

int main(void) {
    valid_flow_test();
    short_hold_test();
    hold_500_to_530_test();
    cancel_tests();
    timeout_test();
    replay_test();
    unknown_ack_test();
    result_key_rearm_test();
    puts("voice_fsm: all timing and protocol tests PASS");
    return 0;
}

static void unknown_ack_test(void) {
    setup();
    int64_t down = 9000000;
    complete_to_recording(down);
    int64_t release = down + 1000000;
    event(VOICE_EV_A_UP, release);
    voice_event_t result;
    memset(&result, 0, sizeof(result));
    result.type = VOICE_EV_RESULT;
    result.result = VOICE_RESULT_UNKNOWN;
    voice_fsm_handle(&fsm, &result, release + 100000);
    assert(fsm.state == VOICE_STATE_OUTCOME_UNKNOWN_ACK);
    event(VOICE_EV_A_DOWN, release + 200000);
    assert(fsm.state == VOICE_STATE_READY);
    assert(fsm.result == VOICE_RESULT_NONE);
    event(VOICE_EV_A_UP, release + 300000);
    assert(fsm.state == VOICE_STATE_READY);

    setup();
    complete_to_recording(down);
    event(VOICE_EV_A_UP, release);
    voice_fsm_handle(&fsm, &result, release + 100000);
    assert(fsm.state == VOICE_STATE_OUTCOME_UNKNOWN_ACK);
    voice_fsm_tick(&fsm, release + 11000000);
    assert(fsm.state == VOICE_STATE_READY);
    puts("fsm: UNKNOWN KEY acknowledge and timeout recovery PASS");
}


static void result_key_rearm_test(void) {
    setup();
    int64_t down = 14000000;
    complete_to_recording(down);
    int64_t release = down + 1000000;
    event(VOICE_EV_A_UP, release);
    voice_event_t result;
    memset(&result, 0, sizeof(result));
    result.type = VOICE_EV_RESULT;
    result.result = VOICE_RESULT_SUCCESS;
    voice_fsm_handle(&fsm, &result, release + 100000);
    assert(fsm.state == VOICE_STATE_RESULT);
    event(VOICE_EV_A_DOWN, release + 200000);
    assert(fsm.state == VOICE_STATE_ARMING);
    assert(fsm.result == VOICE_RESULT_NONE);
    event(VOICE_EV_A_UP, release + 300000);
    assert(fsm.state == VOICE_STATE_READY);
    puts("fsm: KEY immediately rearms PTT from RESULT PASS");
}
