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

// Compute the difference in full months and remaining days between two dates.
// Returns (months, days). If `before` is true, the taken date is before the reference.
static void date_diff(const DateParts &ref, const DateParts &taken, int &months, int &days, bool &before) {
  // Normalize to a serial-like comparison
  int ry = ref.year, rm = ref.month, rd = ref.day;
  int ty = taken.year, tm = taken.month, td = taken.day;

  // Determine direction
  if (ry < ty || (ry == ty && (rm < tm || (rm == tm && rd < td)))) {
    before = false; // taken is after reference
  } else {
    before = true;  // taken is before (or equal to) reference
  }

  // Swap so we compute |ref - taken| with the later date as "end"
  int ey, em, ed, sy, sm, sd;
  if (before) {
    // ref is later, taken is earlier
    ey = ry; em = rm; ed = rd;
    sy = ty; sm = tm; sd = td;
  } else {
    ey = ty; em = tm; ed = td;
    sy = ry; sm = rm; sd = rd;
  }

  months = (ey - sy) * 12 + (em - sm);
  int rem_days = ed - sd;
  if (rem_days < 0) {
    months--;
    rem_days += days_in_month(ey - (em > 1 ? 1 : 0), em > 1 ? em - 1 : 12);
  }
  days = rem_days;
  if (months < 0) months = 0;
}

// Format the caption: "5 Monate, 3 Tage" / "vor 5 Monate, 3 Tage" / "1 Monat, 1 Tag"
static void format_caption(int months, int days, bool before, char *out, size_t out_len) {
  std::string s;
  if (before) s += "vor ";
  if (months > 0) {
    s += std::to_string(months);
    s += months == 1 ? " Monat, " : " Monate, ";
  }
  s += std::to_string(days);
  s += days == 1 ? " Tag" : " Tage";
  std::snprintf(out, out_len, "%s", s.c_str());
}

class SnapSaver : public tessera::Saver {
 public:
  void show(const tessera::SaverContext &c) override {
    // Store options
    height_ = c.height;
    std::snprintf(ref_date_, sizeof(ref_date_), "%s", c.options["reference_date"] | "2025-01-01");
    show_button_ = c.options["show_button"] | true;
    std::snprintf(button_service_, sizeof(button_service_), "%s", c.options["button_service"] | "image.reload");

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

    // Optional tap button: bottom-right, round, takes taps
    if (show_button_) {
      const int key = ui::mm(12);
      btn_ = ui::block(c.parent, theme::RAISED);
      lv_obj_set_style_radius(btn_, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_size(btn_, key, key);
      lv_obj_set_pos(btn_, c.width - key - ui::mm(6), c.height - key - ui::mm(6));
      // Icon: a simple refresh/shuffle symbol from Tessera's set
      lv_obj_t *icon = ui::label(btn_, Font::ICON, theme::INK);
      lv_obj_set_width(icon, key);
      lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_pos(icon, 0, (key - ui::line_height(Font::ICON)) / 2);
      ui::set_text(icon, ui::icon(0xF021B));  // refresh icon
      ui::takes_taps(btn_);
    }
  }

  void on_state(JsonObjectConst data) override {
    // Store entity id for actions
    std::snprintf(entity_, sizeof(entity_), "%s", data["entity"] | "");

    // Update picture
    auto attrs = data["attributes"];
    if (attrs && attrs["entity_picture"] | "") {
      if (pic_) ui::set_picture(pic_, attrs["entity_picture"], tessera::Fit::COVER);
    }

    // Compute caption from taken_at
    // An attribute whose name ends in _at and holds a moment comes as seconds since 1970.
    long taken_epoch = attrs ? (long) (attrs["taken_at"] | 0) : 0;
    if (taken_epoch > 0) {
      auto t = tessera::local_time((uint32_t) taken_epoch);
      DateParts taken{t.year, t.month, t.day, t.hour, t.minute};
      DateParts ref;
      if (parse_date(ref_date_, ref)) {
        int months, days;
        bool before;
        date_diff(ref, taken, months, days, before);
        char caption[64];
        format_caption(months, days, before, caption, sizeof(caption));
        ui::set_text(caption_, caption);
        // Size the shade to fit the text
        int tw = ui::text_width(caption, Font::BODY) + 2 * ui::mm(2);
        int th = ui::line_height(Font::BODY) + 2 * ui::mm(1);
        lv_obj_set_size(shade_, tw, th);
        lv_obj_set_pos(shade_, ui::mm(6), height_ - th - ui::mm(6));
        lv_obj_set_width(caption_, tw - 2 * ui::mm(2));
        lv_obj_set_pos(caption_, ui::mm(2), ui::mm(1));
        lv_obj_remove_flag(shade_, LV_OBJ_FLAG_HIDDEN);
        return;
      }
    }
    // No caption available
    lv_obj_add_flag(shade_, LV_OBJ_FLAG_HIDDEN);
  }

  void on_tap() override {
    if (!btn_) return;
    auto p = tessera::tap_point();
    lv_area_t a;
    lv_obj_get_coords(btn_, &a);
    const int slack = ui::px(6);
    if (p.x >= a.x1 - slack && p.x <= a.x2 + slack && p.y >= a.y1 - slack && p.y <= a.y2 + slack) {
      if (entity_[0]) {
        tessera::action(button_service_, entity_);
      }
    }
  }

 private:
  int height_ = 0;
  bool show_button_ = true;
  char ref_date_[32] = "2025-01-01";
  char button_service_[64] = "image.reload";
  char entity_[128] = "";
  lv_obj_t *pic_{}, *caption_{}, *shade_{}, *btn_{};
};

void TimedMemories::setup() {
  add_saver("snap", tessera::SaverStep::OWN, []() { return new SnapSaver(); });
}

}  // namespace esphome::timed_memories