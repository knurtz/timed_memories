# Timed Memories
A Tessera screensaver step that shows a camera or image entity full-screen with a caption showing the subject's age on the day the photo was taken.

## What it looks like

The photo fills the whole glass, centred and cropped only as needed to cover it. In the bottom-left corner, a small caption shows the age in months and days from the configured birthday to the photo's `taken_at` date, in the screen's own language, like "5 months, 3 days" or "5 Monate, 3 Tage". Both parts always show, so a photo from the birthday reads "0 months, 0 days". A photo without a taken date gets a short word in its place. An optional round button in the bottom-right corner runs an action while the screen stays in standby.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. Add your Immich Picture camera entity to the Screensaver step "Timed Memories". For the age to show, it must expose a capture date in `taken_at` (the plugin also accepts `date_taken`, `created_at`, `date`, and `date_created`).
3. Set the Birthday as year-month-day. The caption counts from it to the photo date.
4. Optionally enter a script or automation id as `script.<name>` or `automation.<name>` (as Home Assistant shows it in Developer Tools). Its button appears only while this field is set.

## Immich Picture camera

Use the Immich Picture camera entity rather than Collection Image. Its thumbnail is expected to remain below Tessera's
4 MB entity-picture download limit and it supplies `taken_at`. If the entity has no `entity_picture` URL, the saver shows
"No image address". Tessera currently does not send plugin savers the reason when its add-on cannot fetch, size-check,
or decode a non-empty image URL; consult the Tessera add-on log for those cases.

