# Changelog

## 0.3.2

- Target Immich Picture camera entities: retain their capture date and use their thumbnail as the full-screen image.
- Make the image place square, clear it when the entity has no picture, and accept the common capture-date attributes as
  either an epoch or an ISO-like date string.
- Resolve button icons from Tessera's complete installed icon set, with or without `mdi:` or as a supported five-digit
  hexadecimal codepoint.
- Keep only the birthday, button action, and button icon options; a button appears only for a configured script or automation.
- Show an on-screen diagnostic when Home Assistant supplies no image address.

## 0.3.0

- The photo again fills the whole screen, the way the photo screensaver does: on a board with pictures the snapshot
  stands behind the caption and the button.
- The caption and the button now look like the built-in music screensaver. The caption is laid over the photo at the
  bottom left, with no dark panel behind it. The button is the round key in the bottom right corner: a soft white
  circle with a white sign, brighter under a finger.
- A new "Button icon" option takes a Material Design Icons name (such as `skip-next`, `play`, `shuffle-variant` or
  `heart`). While it is empty or names an icon the screen cannot draw, a default (`skip-next`) stands in for it. The
  button now shows whenever "Show button" is on, even while no action is set.

## 0.2.0

- The caption now counts whole months and days exactly (the old calculation went wrong near the end of a month)
  and reads as a plain age in the screen's language, like "5 months, 3 days" or "5 Monate, 3 Tage" (think of the
  reference date as a birthday: the caption then says how old the subject in the photo is). The reference day
  itself reads "0 months, 0 days".
- A photo without a taken moment shows the screen's own word instead of no caption at all.
- The button now runs a script or an automation: type its id as `script.<name>` or `automation.<name>` (as you see it
  in Developer Tools). Anything else is a service as `domain.action`, e.g. `image.reload`, for the camera itself.
  While the field is empty the button stays out. Update: fill the field in, or hide the button with "Show button" off.

## 0.1.0

- Initial release: full-screen camera/image snapshot with age caption and optional action button.
