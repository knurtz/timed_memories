# Timed Memories
A Tessera screensaver step that shows a photo full-screen with a caption showing how long ago it was taken relative to a reference date you choose.

## What it looks like

The photo fills the whole glass. In the bottom-left corner, a small caption shows the age in months and days from the reference date to the day the photo was taken, in the screen's own language, like "5 months, 3 days" or "5 Monate, 3 Tage". A birthday works well as the reference date: the caption then reads how old the subject in the photo is. Both parts always show, so the reference day itself reads "0 months, 0 days". A photo without a taken date gets a short word in its place. An optional round button in the bottom-right corner runs an action while the screen stays in standby.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. Add an image or camera entity to the Screensaver step "Timed Memories". For the age to show, the entity's `taken_at` attribute must hold its taken moment.
3. Set the Reference date, as year-month-day. The caption counts from it to the photo.
4. For the button, type a script or automation id as `script.<name>` or `automation.<name>` (as Home Assistant shows it in Developer Tools), or a service as `domain.action`, e.g. `image.reload`, for the camera itself. Leave it empty for no button at all.

