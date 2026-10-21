#include "timed_memories.h"
#include "esphome/components/smart_display/tile_icon.h"
#include "esphome/components/smart_display/tile_icon_names.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::timed_memories {

using tessera::Font;
namespace ui = tessera::ui;

// Parse "YYYY-MM-DD" or "YYYY-MM-DD HH:MM:SS" into a simple struct.
struct DateParts {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
};

static int days_in_month(int y, int m);

static bool parse_date(const char *s, DateParts &d) {
  if (!s || !*s) return false;
  int y, m, dd, h = 0, mi = 0;
  if (sscanf(s, "%d-%d-%d %d:%d", &y, &m, &dd, &h, &mi) >= 3) {
    if (y > 0 && m >= 1 && m <= 12 && dd >= 1 && dd <= days_in_month(y, m)) {
      d.year = y; d.month = m; d.day = dd; d.hour = h; d.minute = mi;
      return true;
    }
  }
  if (sscanf(s, "%d/%d/%d %d:%d", &y, &m, &dd, &h, &mi) >= 3) {
    if (y > 0 && m >= 1 && m <= 12 && dd >= 1 && dd <= days_in_month(y, m)) {
      d.year = y; d.month = m; d.day = dd; d.hour = h; d.minute = mi;
      return true;
    }
  }
  return false;
}

// Days in a month (non-leap; leap correction added separately).
static int days_in_month(int y, int m) {
  static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m < 1 || m > 12) return 30;
  if (m == 2) {
    bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    return leap ? 29 : 28;
  }
  return dim[m - 1];
}

// Days since 1970-01-01 of a civil date (Howard Hinnant's days_from_civil, as the core uses beside
// tessera::days_from_today), so two dates subtract to whole days, leap years included.
static int32_t civil_days(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

static int32_t serial(const DateParts &d) { return civil_days(d.year, d.month, d.day); }

// `months` whole months after the date, its day clamped to the end of a shorter month (Jan 31 plus one
// month is Feb 28). months is 0 or more.
static DateParts add_months(const DateParts &d, int months) {
  const int total = d.month - 1 + months;
  int y = d.year + total / 12;
  int m = total % 12;
  if (m < 0) { y--; m += 12; }
  DateParts out;
  out.year = y;
  out.month = m + 1;
  const int last = days_in_month(y, m + 1);
  out.day = d.day < last ? d.day : last;
  return out;
}

// The whole months and the days that follow, from the earlier date to the later one.
static void date_diff(const DateParts &earlier, const DateParts &later, int &months, int &days) {
  months = 0;
  for (;;) {
    if (serial(add_months(earlier, months + 1)) > serial(later)) break;
    months++;
  }
  days = (int) (serial(later) - serial(add_months(earlier, months)));
}

// The caption from the screen's words (translations/<lang>.json, part "screen"): a plain age in months and
// days, both parts always ("0 months, 0 days" for none). "" when the screen has no word for it.
static std::string caption(const tessera::Plugin *plugin, int months, int days) {
  const char *key = months == 1 ? (days == 1 ? "month_day" : "month_days")
                                : (days == 1 ? "months_day" : "months_days");
  std::string text(plugin->text(key));
  if (text.empty()) return "";
  if (months != 1) text = tessera::fill(text.c_str(), "months", std::to_string(months));
  if (days != 1) text = tessera::fill(text.c_str(), "days", std::to_string(days));
  return text;
}

// The icon the button shows, by the Material Design Icons name the person types in "Button icon". Only the names
// the screen's icon font carries resolve to a glyph (codepoints from the same set the built-in icons draw from); a
// name it has not, or no name at all, gives 0 and the button falls back to its default (skip-next, 0xF04AD).
static uint32_t button_icon(const char *raw) {
  std::string name = raw ? raw : "";
  const auto first = std::find_if_not(name.begin(), name.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last = std::find_if_not(name.rbegin(), name.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  name = first < last ? std::string(first, last) : std::string();
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (name.rfind("mdi:", 0) == 0) name.erase(0, 4);
  if (const uint32_t codepoint = tile_icon::named(name)) return codepoint;
  const uint32_t codepoint = tile_icon::codepoint(name);
  return tile_icon::carried(codepoint) ? codepoint : 0;
}

// The add-on turns attributes ending in _at or _date into local epoch seconds. Other integrations may send a date
// string instead, so accept both forms and the common names that describe when a photo was made.
static bool picture_date(JsonObjectConst attrs, DateParts &date) {
  if (!attrs) return false;
  static constexpr const char *names[] = {"taken_at", "date_taken", "created_at", "date", "date_created"};
  for (const char *name : names) {
    const JsonVariantConst value = attrs[name];
    const uint32_t epoch = value | 0u;
    if (epoch) {
      const auto t = tessera::local_time(epoch);
      date = {t.year, t.month, t.day, t.hour, t.minute};
      return true;
    }
    if (parse_date(value | "", date)) return true;
  }
  return false;
}

class SnapSaver : public tessera::Saver {
 public:
  explicit SnapSaver(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void show(const tessera::SaverContext &c) override {
    width_ = c.width;
    height_ = c.height;
    std::snprintf(ref_date_, sizeof(ref_date_), "%s", c.options["reference_date"] | "2025-01-01");
    std::snprintf(button_action_, sizeof(button_action_), "%s", c.options["button_action"] | "");
    std::snprintf(icon_, sizeof(icon_), "%s", c.options["button_icon"] | "");

    // The photo: the entity's own picture over the whole glass, made first so the caption and the button lie over it.
    if (ui::pictures()) {
      pic_ = ui::picture(c.parent);
      lv_obj_set_size(pic_, c.width, c.height);
      lv_obj_set_pos(pic_, 0, 0);
      lv_obj_set_style_radius(pic_, 0, 0);
    }

    // The caption: the age of the subject, laid over the picture the way the music saver lays its words (no shade).
    caption_ = ui::label(c.parent, Font::BODY_LARGE, theme::INK_SOFT);
    lv_obj_add_flag(caption_, LV_OBJ_FLAG_HIDDEN);

    // A local diagnostic is possible when the entity did not give the app a picture address. Fetch, size and decode
    // failures happen in the add-on after this saver has asked for the picture; plugin API 0.14 has no callback for
    // those failures, so do not claim a particular cause here.
    problem_ = ui::label(c.parent, Font::BODY_LARGE, theme::INK_SOFT);
    lv_obj_add_flag(problem_, LV_OBJ_FLAG_HIDDEN);

    // The button only exists when it can run an action; otherwise every tap outside the photo wakes the screen.
    if (button_action_[0]) make_button(c.parent, c.width, c.height);
  }

  void on_state(JsonObjectConst data) override {
    // The photo: the entity's own picture, whole glass, filling it (COVER), as the photos step shows its photo.
    auto attrs = data["attributes"];
    const char *pic = attrs ? (attrs["entity_picture"] | "") : "";
    if (pic_) ui::set_picture(pic_, pic, tessera::Fit::COVER);
    if (!pic || !pic[0]) {
      show_problem(plugin_->text("no_picture"));
      if (caption_) lv_obj_add_flag(caption_, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    if (problem_) lv_obj_add_flag(problem_, LV_OBJ_FLAG_HIDDEN);

    // The caption: how long ago (or ahead) the photo was taken, from the reference date, in the screen's words.
    // Laid at the bottom left, a margin from the edge, ending before the button, like the music saver's words.
    if (!caption_) return;
    const std::string text = caption_text(attrs);
    if (text.empty()) {
      lv_obj_add_flag(caption_, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    const int margin = ui::px(ui::large() ? 28 : 12);
    const int line = ui::line_height(Font::BODY_LARGE);
    int x = margin, w = width_ - 2 * margin;
    if (btn_) {
      const int key_left = width_ - margin - ui::px(ui::large() ? 64 : 40);
      const int cap = key_left - margin / 2 - x;
      if (cap < w) w = std::max(1, cap);
    }
    ui::set_text(caption_, text);
    lv_obj_set_width(caption_, w);
    lv_obj_set_pos(caption_, x, height_ - margin - line);
    lv_obj_remove_flag(caption_, LV_OBJ_FLAG_HIDDEN);
  }

  // Only the button takes taps (ui::takes_taps), so a tap here is on it. A tap on the photo lets the finger through
  // and wakes the screen, as the rest of a saver's glass does.
  void on_tap() override {
    if (btn_) tap_button();
  }

 private:
  // What the button runs: a script or automation by its entity id. The manifest declares both services so Tessera
  // can show a person what the plugin may do before it is installed.
  void tap_button() {
    if (strncmp(button_action_, "script.", 7) == 0) {
      tessera::action("script.turn_on", button_action_);
    } else if (strncmp(button_action_, "automation.", 11) == 0) {
      tessera::action("automation.trigger", button_action_);
    }
  }

  void show_problem(const char *text) {
    if (!problem_ || !text || !text[0]) return;
    const int margin = ui::px(ui::large() ? 28 : 12);
    ui::set_text(problem_, text);
    lv_obj_set_width(problem_, std::max(1, width_ - 2 * margin));
    lv_obj_set_style_text_align(problem_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(problem_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(problem_, LV_OBJ_FLAG_HIDDEN);
  }

  // The caption to show, "" for none. The screen's own word while the photo has no taken moment, whose
  // value of the attribute (a moment) comes as seconds since 1970.
  std::string caption_text(JsonObjectConst attrs) {
    DateParts ref;
    if (!parse_date(ref_date_, ref)) return "";
    DateParts taken;
    if (!picture_date(attrs, taken)) {
      // A photo without a moment: the screen's word for it.
      const char *word = plugin_->text("no_date");
      return word && word[0] ? std::string(word) : std::string();
    }
    int months = 0, days = 0;
    // The age, from the reference date to the photo, in whichever direction a wrong setting stands.
    if (serial(ref) <= serial(taken)) date_diff(ref, taken, months, days);
    else date_diff(taken, ref, months, days);
    return caption(plugin_, months, days);
  }

  // The button: one round key the music saver stands in the bottom right, drawn as its side keys are (a white haze
  // over the picture, a white sign, brighter under a finger). Its icon is the option's, or the default (skip-next)
  // while it is empty or names an icon the font does not carry.
  void make_button(lv_obj_t *parent, int width, int height) {
    const int margin = ui::px(ui::large() ? 28 : 12);
    const int key = ui::px(ui::large() ? 64 : 40);
    btn_ = ui::block(parent, theme::CAMERA_INK);
    lv_obj_set_style_radius(btn_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(btn_, 56, 0);
    lv_obj_set_style_bg_opa(btn_, 110, LV_STATE_PRESSED);
    lv_obj_set_size(btn_, key, key);
    lv_obj_set_pos(btn_, width - margin - key, height - margin - key);
    lv_obj_t *icon = ui::label(btn_, Font::ICON, theme::CAMERA_INK);
    lv_obj_set_width(icon, key);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(icon, 0, (key - ui::line_height(Font::ICON)) / 2);
    const uint32_t codepoint = button_icon(icon_);
    ui::set_text(icon, ui::icon(codepoint ? codepoint : 0xF04AD));
    ui::takes_taps(btn_);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  char ref_date_[32] = "2025-01-01";
  char button_action_[64] = "";
  char icon_[32] = "";
  lv_obj_t *pic_{}, *caption_{}, *problem_{}, *btn_{};
};

void TimedMemories::setup() {
  // The component is the plugin, so the saver reaches its words (translations, part "screen") through it.
  add_saver("snap", tessera::SaverStep::OWN, [this]() { return new SnapSaver(this); });
}

}  // namespace esphome::timed_memories
