# Changelog

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