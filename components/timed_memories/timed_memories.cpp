#include "timed_memories.h"
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

static bool parse_date(const char *s, DateParts &d) {
  if (!s || !*s) return false;
  int y, m, dd, h = 0, mi = 0;
  if (sscanf(s, "%d-%d-%d %d:%d", &y, &m, &dd, &h, &mi) >= 3) {
    d.year = y; d.month = m; d.day = dd; d.hour = h; d.minute = mi;
    return true;
  }
  if (sscanf(s, "%d/%d/%d %d:%d", &y, &m, &dd, &h, &mi) >= 3) {
    d.year = y; d.month = m; d.day = dd; d.hour = h; d.minute = mi;
    return true;
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

class SnapSaver : public tessera::Saver {
 public:
  explicit SnapSaver(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void show(const tessera::SaverContext &c) override {
    // Store options
    height_ = c.height;
    std::snprintf(ref_date_, sizeof(ref_date_), "%s", c.options["reference_date"] | "2025-01-01");
    show_button_ = c.options["show_button"] | true;
    std::snprintf(button_action_, sizeof(button_action_), "%s", c.options["button_action"] | "");

    // Full-screen picture
    if (ui::pictures()) {
      pic_ = ui::picture(c.parent);
      lv_obj_set_size(pic_, c.width, c.height);
      lv_obj_set_pos(pic_, 0, 0);
    }

    // Caption label: bottom-left, small, with a semi-transparent shade behind it
    shade_ = ui::block(c.parent, theme::CAMERA_PAGE);
    lv_obj_set_style_bg_opa(shade_, LV_OPA_50, 0);
    lv_obj_add_flag(shade_, LV_OBJ_FLAG_HIDDEN);
    caption_ = ui::label(shade_, Font::BODY, theme::INK);
    lv_obj_set_width(caption_, 1);

    // The tap button: bottom-right, round, takes taps; only when an action is named.
    if (show_button_ && button_action_[0]) make_button(c.parent, c.width, c.height);
  }

  void on_state(JsonObjectConst data) override {
    // Store entity id for actions
    std::snprintf(entity_, sizeof(entity_), "%s", data["entity"] | "");

    // Update picture
    auto attrs = data["attributes"];
    if (attrs && attrs["entity_picture"] | "") {
      if (pic_) ui::set_picture(pic_, attrs["entity_picture"], tessera::Fit::COVER);
    }

    // The caption: how long ago (or ahead) the photo was taken, from the reference date, in the screen's words.
    const std::string text = caption_text(attrs);
    if (text.empty()) {
      lv_obj_add_flag(shade_, LV_OBJ_FLAG_HIDDEN);
      return;
    }
    ui::set_text(caption_, text);
    // Size the shade to fit the text
    int tw = ui::text_width(text, Font::BODY) + 2 * ui::mm(2);
    int th = ui::line_height(Font::BODY) + 2 * ui::mm(1);
    lv_obj_set_size(shade_, tw, th);
    lv_obj_set_pos(shade_, ui::mm(6), height_ - th - ui::mm(6));
    lv_obj_set_width(caption_, tw - 2 * ui::mm(2));
    lv_obj_set_pos(caption_, ui::mm(2), ui::mm(1));
    lv_obj_remove_flag(shade_, LV_OBJ_FLAG_HIDDEN);
  }

  void on_tap() override {
    if (!btn_) return;
    auto p = tessera::tap_point();
    lv_area_t a;
    lv_obj_get_coords(btn_, &a);
    const int slack = ui::px(6);
    if (p.x >= a.x1 - slack && p.x <= a.x2 + slack && p.y >= a.y1 - slack && p.y <= a.y2 + slack) tap_button();
  }

 private:
  // What the button runs: a script or an automation by its entity id (the service that goes with the domain), or,
  // without one, a service as domain.action (such as image.reload) on the saver's own entity.
  void tap_button() {
    if (strncmp(button_action_, "script.", 7) == 0) {
      tessera::action("script.turn_on", button_action_);
    } else if (strncmp(button_action_, "automation.", 11) == 0) {
      tessera::action("automation.trigger", button_action_);
    } else if (strchr(button_action_, '.')) {
      if (entity_[0]) tessera::action(button_action_, entity_);
    }
  }

  // The caption to show, "" for none. The screen's own word while the photo has no taken moment, whose
  // value of the attribute (a moment) comes as seconds since 1970.
  std::string caption_text(JsonObjectConst attrs) {
    long taken_epoch = attrs ? (long) (attrs["taken_at"] | 0) : 0;
    DateParts ref;
    if (!parse_date(ref_date_, ref)) return "";
    if (!(taken_epoch > 0)) {
      // A photo without a moment: the screen's word for it.
      const char *word = plugin_->text("no_date");
      return word && word[0] ? std::string(word) : std::string();
    }
    auto t = tessera::local_time((uint32_t) taken_epoch);
    DateParts taken{t.year, t.month, t.day, t.hour, t.minute};
    int months = 0, days = 0;
    // The age, from the reference date to the photo, in whichever direction a wrong setting stands.
    if (serial(ref) <= serial(taken)) date_diff(ref, taken, months, days);
    else date_diff(taken, ref, months, days);
    return caption(plugin_, months, days);
  }

  void make_button(lv_obj_t *parent, int width, int height) {
    const int key = ui::mm(12);
    btn_ = ui::block(parent, theme::RAISED);
    lv_obj_set_style_radius(btn_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(btn_, key, key);
    lv_obj_set_pos(btn_, width - key - ui::mm(6), height - key - ui::mm(6));
    // Icon: a simple refresh/shuffle symbol from Tessera's set
    lv_obj_t *icon = ui::label(btn_, Font::ICON, theme::INK);
    lv_obj_set_width(icon, key);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(icon, 0, (key - ui::line_height(Font::ICON)) / 2);
    ui::set_text(icon, ui::icon(0xF021B));  // refresh icon
    ui::takes_taps(btn_);
  }

  const tessera::Plugin *plugin_;
  int height_ = 0;
  bool show_button_ = true;
  char ref_date_[32] = "2025-01-01";
  char button_action_[64] = "";
  char entity_[128] = "";
  lv_obj_t *pic_{}, *caption_{}, *shade_{}, *btn_{};
};

void TimedMemories::setup() {
  // The component is the plugin, so the saver reaches its words (translations, part "screen") through it.
  add_saver("snap", tessera::SaverStep::OWN, [this]() { return new SnapSaver(this); });
}

}  // namespace esphome::timed_memories