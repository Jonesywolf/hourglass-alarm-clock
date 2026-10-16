#ifndef HG_SERVICES_EFFECTS_H
#define HG_SERVICES_EFFECTS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HG_EFFECT_NORMAL = 0,
    HG_EFFECT_SUNRISE,
    HG_EFFECT_RINGING
} hg_effect_phase_t;

typedef enum {
    HG_FEEDBACK_NONE = 0,
    HG_FEEDBACK_NAVIGATE,
    HG_FEEDBACK_ALARM_EDIT,
    HG_FEEDBACK_ALARM_ENTER,
    HG_FEEDBACK_ALARM_FIELD,
    HG_FEEDBACK_ALARM_SAVE,
    HG_FEEDBACK_ALARM_ENABLE,
    HG_FEEDBACK_ALARM_DISABLE
} hg_feedback_t;

typedef enum {
    HG_FEEDBACK_ACCEPTED = 0,
    HG_FEEDBACK_UNAVAILABLE,
    HG_FEEDBACK_PREEMPTED,
    HG_FEEDBACK_RATE_LIMITED
} hg_feedback_result_t;

typedef struct {
    hg_effect_phase_t phase;
    hg_feedback_t feedback;
    uint32_t generation;
    bool led_available;
    bool audio_available;
    bool leds_active;
    bool leds_starting;
    bool audio_active;
    bool stop_pending;
    bool pause_indoor;
    bool timing_sensitive;
} hg_effects_snapshot_t;

/* Policy is UI-task-owned. Output workers receive copies, not this state. */
void hg_effects_init(void);
void hg_effects_set_capabilities(bool led, bool audio);
void hg_effects_set_alarm(bool sunrise, bool ringing);
void hg_effects_set_pause_indoor_for_leds(bool enabled);
hg_feedback_result_t hg_effects_request_feedback(hg_feedback_t feedback, uint32_t now_ms);
void hg_effects_update(uint32_t now_ms);
void hg_effects_cancel_feedback(void);
bool hg_effects_report_applied(uint32_t generation, bool leds_active,
                               bool audio_active, bool leds_starting);
void hg_effects_get_snapshot(hg_effects_snapshot_t *out);

#endif
