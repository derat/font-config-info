#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include <fontconfig/fontconfig.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

#define NAME_FORMAT "%-20s "

const char* GetFontconfigHintStyleString(int style) {
  switch (style) {
    case FC_HINT_NONE:
      return "none";
    case FC_HINT_SLIGHT:
      return "slight";
    case FC_HINT_MEDIUM:
      return "medium";
    case FC_HINT_FULL:
      return "full";
    default:
      return "invalid";
  }
}

const char* GetFontconfigRgbaString(int rgba) {
  switch (rgba) {
    case FC_RGBA_UNKNOWN:
      return "unknown";
    case FC_RGBA_RGB:
      return "rgb";
    case FC_RGBA_BGR:
      return "bgr";
    case FC_RGBA_VRGB:
      return "vrgb";
    case FC_RGBA_VBGR:
      return "vbgr";
    case FC_RGBA_NONE:
      return "none";
    default:
      return "invalid";
  }
}

void PrintGtkBoolSetting(GtkSettings* settings, const char* name) {
  gint value = -1;
  g_object_get(settings, name, &value, NULL);
  printf(NAME_FORMAT "%d (%s)\n", name, value,
         (value == 0 ? "no": (value > 0 ? "yes" : "default")));
}

void PrintGtkStringSetting(GtkSettings* settings, const char* name) {
  gchar* value = NULL;
  g_object_get(settings, name, &value, NULL);
  printf(NAME_FORMAT "\"%s\"\n", name, (value ? value : "[unset]"));
  if (value)
    g_free(value);
}

void PrintGtkSettings() {
  printf("GtkSettings:\n");
  GtkSettings* settings = gtk_settings_get_default();
  assert(settings);
  PrintGtkStringSetting(settings, "gtk-font-name");
  PrintGtkBoolSetting(settings, "gtk-xft-antialias");
  PrintGtkBoolSetting(settings, "gtk-xft-hinting");
  PrintGtkStringSetting(settings, "gtk-xft-hintstyle");
  PrintGtkStringSetting(settings, "gtk-xft-rgba");

  // The DPI setting actually contains the real DPI times 1024.
  const char kDpiName[] = "gtk-xft-dpi";
  gint dpi = -1;
  g_object_get(settings, kDpiName, &dpi, NULL);
  if (dpi > 0)
    printf(NAME_FORMAT "%d (%0.2f DPI)\n", kDpiName, dpi, dpi / 1024.0);
  else
    printf(NAME_FORMAT "%d (default)\n", kDpiName, dpi);

  printf("\n");
}

void PrintGtkWidgetFontStyleAndSinkRef(GtkWidget* widget) {
  GtkStyle* style = gtk_rc_get_style(widget);
  PangoFontDescription* font_desc = style->font_desc;
  gchar* font_string = font_desc ?
      pango_font_description_to_string(font_desc) : NULL;
  printf(NAME_FORMAT "\"%s\"\n", G_OBJECT_TYPE_NAME(widget),
         font_string ? font_string : "[unset]");

  if (font_string)
    g_free(font_string);
  g_object_ref_sink(widget);
}

void PrintGtkStyles() {
  printf("GTK 2.0 styles:\n");
  PrintGtkWidgetFontStyleAndSinkRef(gtk_label_new("foo"));
  PrintGtkWidgetFontStyleAndSinkRef(gtk_menu_item_new_with_label("foo"));
  PrintGtkWidgetFontStyleAndSinkRef(gtk_toolbar_new());
  printf("\n");
}

void PrintGSettingsSetting(GSettings* settings,
                           const char* key) {
  GVariant* variant = g_settings_get_value(settings, key);
  if (!variant) {
    printf(NAME_FORMAT "[unset]\n", key);
    return;
  }

  if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING))
    printf(NAME_FORMAT "\"%s\"\n", key, g_variant_get_string(variant, NULL));
  else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_DOUBLE))
    printf(NAME_FORMAT "%0.2f\n", key, g_variant_get_double(variant));
  else
    printf(NAME_FORMAT "[unknown type]\n", key);

  g_variant_unref(variant);
}

void PrintGnomeSettings() {
  const char kSchema[] = "org.gnome.desktop.interface";
  printf("GSettings (%s):\n", kSchema);
  GSettings* settings = g_settings_new(kSchema);
  assert(settings);
  PrintGSettingsSetting(settings, "font-name");
  PrintGSettingsSetting(settings, "text-scaling-factor");
  g_object_unref(settings);
  printf("\n");
}

void PrintXDisplayInfo() {
  printf("X11 display info:\n");
  Display* display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
  assert(display);

  const int screen = DefaultScreen(display);
  const int width_px = DisplayWidth(display, screen);
  const int height_px = DisplayHeight(display, screen);
  const int width_mm = DisplayWidthMM(display, screen);
  const int height_mm = DisplayHeightMM(display, screen);
  const double x_dpi = width_px * 25.4 / width_mm;
  const double y_dpi = height_px * 25.4 / height_mm;

  printf(NAME_FORMAT "%dx%d\n", "screen pixels", width_px, height_px);
  printf(NAME_FORMAT "%dx%d mm (%.2fx%.2f DPI)\n", "screen size",
         width_mm, height_mm, x_dpi, y_dpi);
  printf("\n");
}

void PrintXResource(XrmDatabase db, const char* name) {
  char* type = NULL;
  XrmValue value;
  if (!XrmGetResource(db, name, "*", &type, &value)) {
    printf(NAME_FORMAT "[unset]\n", name);
    return;
  }

  const size_t kBuffer = 256;
  char str[kBuffer];
  size_t size = value.size < kBuffer ? value.size : kBuffer;
  strncpy(str, value.addr, size);
  str[kBuffer - 1] = '\0';
  printf(NAME_FORMAT "\"%s\"\n", name, str);
}

void PrintXResources() {
  printf("X resources (xrdb):\n");
  Display* display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
  assert(display);

  const char* data = XResourceManagerString(display);
  if (!data) {
    printf("[failed]\n\n");
    return;
  }

  XrmDatabase db = XrmGetStringDatabase(data);
  PrintXResource(db, "Xft.antialias");
  PrintXResource(db, "Xft.hinting");
  PrintXResource(db, "Xft.hintstyle");
  PrintXResource(db, "Xft.rgba");
  PrintXResource(db, "Xft.dpi");
  XrmDestroyDatabase(db);
  printf("\n");
}

void PrintFontconfigSettings() {
  GtkWidget* widget = gtk_label_new("foo");
  PangoFontDescription* desc = gtk_rc_get_style(widget)->font_desc;

  gchar* desc_string = pango_font_description_to_string(desc);
  printf("Fontconfig (%s):\n", desc_string);
  g_free(desc_string);

  FcPattern* pattern = FcPatternCreate();
  assert(pattern);

  FcValue value;
  value.type = FcTypeString;
  value.u.s = (const FcChar8*) pango_font_description_get_family(desc);
  FcPatternAdd(pattern, FC_FAMILY, value, FcTrue /* append */);

  PangoContext* pango_context = pango_font_map_create_context(
      pango_cairo_font_map_get_default());
  double pango_dpi = pango_cairo_context_get_resolution(pango_context);
  g_object_unref(pango_context);
  if (pango_dpi <= 0)
    pango_dpi = 96.0;
  const double pixels_in_point = pango_dpi / 72.0;
  double desc_pixel_size = pango_font_description_get_size(desc);
  if (!pango_font_description_get_size_is_absolute(desc))
    desc_pixel_size *= pixels_in_point;
  desc_pixel_size /= PANGO_SCALE;
  value.type = FcTypeDouble;
  value.u.d = desc_pixel_size;
  FcPatternAdd(pattern, FC_PIXEL_SIZE, value, FcTrue /* append */);

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  assert(match);

  FcChar8* family = NULL;
  FcPatternGetString(match, FC_FAMILY, 0, &family);
  double pixel_size = 0.0;
  FcPatternGetDouble(match, FC_PIXEL_SIZE, 0, &pixel_size);
  FcBool antialias = 0;
  FcPatternGetBool(match, FC_ANTIALIAS, 0, &antialias);
  FcBool hinting = 0;
  FcPatternGetBool(match, FC_HINTING, 0, &hinting);
  FcBool autohint = 0;
  FcPatternGetBool(match, FC_AUTOHINT, 0, &autohint);
  int hint_style = FC_HINT_NONE;
  FcPatternGetInteger(match, FC_HINT_STYLE, 0, &hint_style);
  int rgba = FC_RGBA_UNKNOWN;
  FcPatternGetInteger(match, FC_RGBA, 0, &rgba);

  printf(NAME_FORMAT "\"%s\"\n", "FC_FAMILY", family);
  printf(NAME_FORMAT "%.2f (requested %.2f at %.2f DPI)\n",
         "FC_PIXEL_SIZE", pixel_size, desc_pixel_size, pango_dpi);
  printf(NAME_FORMAT "%d\n", "FC_ANTIALIAS", antialias);
  printf(NAME_FORMAT "%d\n", "FC_HINTING", hinting);
  printf(NAME_FORMAT "%d\n", "FC_AUTOHINT", autohint);
  printf(NAME_FORMAT "%d (%s)\n", "FC_HINT_STYLE", hint_style,
         GetFontconfigHintStyleString(hint_style));
  printf(NAME_FORMAT "%d (%s)\n", "FC_RGBA", rgba,
         GetFontconfigRgbaString(rgba));

  FcPatternDestroy(pattern);
  FcPatternDestroy(match);
  g_object_ref_sink(widget);
  printf("\n");
}

void PrintXSettings() {
  printf("XSETTINGS:\n");
  int retval = system(
      "set -o pipefail; "
      "dump_xsettings | "
        "grep -E '^(Gtk/FontName |Xft/)' | "
        "sed -e 's/  */|/' | "
        "awk -F '|' '{printf \"" NAME_FORMAT "%s\\n\", $1, $2}'");
  if (WEXITSTATUS(retval) != 0) {
    printf("Install dump_xsettings from https://code.google.com/p/xsettingsd/\n"
           "to print this information.\n");
  }
  printf("\n");
}

int main(int argc, char** argv) {
  time_t now = time(NULL);
  printf("Running at %s\n", ctime(&now));

  gtk_init(&argc, &argv);
  PrintGtkSettings();
  PrintGtkStyles();
  PrintGnomeSettings();
  PrintXDisplayInfo();
  PrintXResources();
  PrintXSettings();
  PrintFontconfigSettings();
  return 0;
}
