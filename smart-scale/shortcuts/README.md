# iOS Shortcuts

Per-user shortcuts that bridge Home Assistant weight readings into Apple
HealthKit. One copy lives on each user's iPhone, bound to that user's Apple ID.

## `log-weight-to-health.shortcut` (TBD)

To be exported once tested. Recipe:

1. **Get the contents of URL** —
   `https://<your-ha>/api/states/sensor.scale_user_<n>_weight`
   with header `Authorization: Bearer <long-lived-access-token>`.
2. **Get dictionary value** for key `state` from the response → numeric weight in kg.
3. **Log Health Sample** — Type: Weight, Value: previous step's number,
   Unit: **kilograms** (force this; do not let it follow device locale).

Trigger:

- **Recommended**: actionable push notification from HA (one-tap "Log to Health"
  button). Apple does not allow inbound webhooks to start a Shortcut.
- Alternative: iOS Personal Automation triggered by time-of-day if you weigh in
  at a consistent time.

First run will prompt iOS for permission to write Weight to Health. Tap allow
once; it's permanent.

## Companion HA automation

Each user gets one automation in Home Assistant that fires on their
`sensor.scale_user_<n>_weight` updating, with `notify.mobile_app_<their_iphone>`
as the action. Example payload:

```yaml
service: notify.mobile_app_alex_iphone
data:
  message: "Weight: {{ states('sensor.scale_user_1_weight') }} kg"
  data:
    actions:
      - action: LOG_TO_HEALTH
        title: "Log to Health"
```

Tapping the action opens the Shortcut.
