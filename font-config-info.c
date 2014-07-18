#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <fontconfig/fontconfig.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

#define NAME_FORMAT "%-20s "

const char* GetFontconfigResultString(FcResult result) {
  switch (result) {
    case FcResultMatch:
      return "match";
    case FcResultNoMatch:
      return "no match";
    case FcResultTypeMismatch:
      return "type mismatch";
    case FcResultNoId:
      return "no id";
    case FcResultOutOfMemory:
      return "out of memory";
    default:
      return "unknown";
  }
}

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

void PrintFontconfigString(FcPattern* match, const char* prop) {
  FcChar8* value = NULL;
  FcResult result = FcResultNoMatch;
  if ((result = FcPatternGetString(match, prop, 0, &value)) != FcResultMatch)
    printf(NAME_FORMAT "[%s]\n", prop, GetFontconfigResultString(result));
  else
    printf(NAME_FORMAT "%s\n", prop, value);
}

void PrintFontconfigBool(FcPattern* match, const char* prop) {
  FcBool value = 0;
  FcResult result = FcResultNoMatch;
  if ((result = FcPatternGetBool(match, prop, 0, &value)) != FcResultMatch)
    printf(NAME_FORMAT "[%s]\n", prop, GetFontconfigResultString(result));
  else
    printf(NAME_FORMAT "%d\n", prop, value);
}

void PrintFontconfigInt(FcPattern* match,
                        const char* prop,
                        const char* (*int_to_string_func)(int),
                        const char* suffix) {
  int value = 0;
  FcResult result = FcResultNoMatch;
  if ((result = FcPatternGetInteger(match, prop, 0, &value)) != FcResultMatch)
    printf(NAME_FORMAT "[%s]\n", prop, GetFontconfigResultString(result));
  else if (int_to_string_func)
    printf(NAME_FORMAT "%d%s (%s)\n", prop, value, suffix, int_to_string_func(value));
  else
    printf(NAME_FORMAT "%d%s\n", prop, value, suffix);
}

void PrintFontconfigDouble(FcPattern* match,
                           const char* prop,
                           const char* suffix) {
  double value = 0.0;
  FcResult result = FcResultNoMatch;
  if ((result = FcPatternGetDouble(match, prop, 0, &value)) != FcResultMatch)
    printf(NAME_FORMAT "[%s]\n", prop, GetFontconfigResultString(result));
  else
    printf(NAME_FORMAT "%.2f%s\n", prop, value, suffix);
}

void PrintFontconfigSettings(const char* user_desc_string,
                             int bold,
                             int italic) {
  PangoFontDescription* desc = NULL;
  if (user_desc_string) {
    desc = pango_font_description_from_string(user_desc_string);
  } else {
    GtkWidget* widget = gtk_label_new("foo");
    desc = pango_font_description_copy(gtk_rc_get_style(widget)->font_desc);
    g_object_ref_sink(widget);
  }

  gchar* desc_string = pango_font_description_to_string(desc);
  printf("Fontconfig (%s):\n", desc_string);
  g_free(desc_string);

  FcPattern* pattern = FcPatternCreate();
  assert(pattern);

  FcPatternAddString(pattern, FC_FAMILY,
                     (const FcChar8*) pango_font_description_get_family(desc));
  if (bold) {
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    printf(NAME_FORMAT "FC_WEIGHT_BOLD\n", "requested weight");
  }
  if (italic) {
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    printf(NAME_FORMAT "FC_SLANT_ITALIC\n", "requested slant");
  }

  // Pass either pixel or points depending on what was requested.
  if (pango_font_description_get_size_is_absolute(desc)) {
    const double pixel_size =
        pango_font_description_get_size(desc) / PANGO_SCALE;
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixel_size);
    printf(NAME_FORMAT "%.2f pixels\n", "requested size", pixel_size);
  } else {
    const int point_size = pango_font_description_get_size(desc) / PANGO_SCALE;
    FcPatternAddInteger(pattern, FC_SIZE, point_size);
    printf(NAME_FORMAT "%d points\n", "requested size", point_size);
  }

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  assert(match);

  int point_size = 0.0;
  FcPatternGetInteger(match, FC_SIZE, 0, &point_size);

  PrintFontconfigString(match, FC_FAMILY);
  PrintFontconfigDouble(match, FC_PIXEL_SIZE, " pixels");
  PrintFontconfigInt(match, FC_SIZE, NULL, " points");
  PrintFontconfigBool(match, FC_ANTIALIAS);
  PrintFontconfigBool(match, FC_HINTING);
  PrintFontconfigBool(match, FC_AUTOHINT);
  PrintFontconfigInt(match, FC_HINT_STYLE, GetFontconfigHintStyleString, "");
  PrintFontconfigInt(match, FC_RGBA, GetFontconfigRgbaString, "");
  printf("\n");

  FcPatternDestroy(pattern);
  FcPatternDestroy(match);
  pango_font_description_free(desc);
}

void PrintXSettings() {
  printf("XSETTINGS:\n");
  int retval = system(
      "bash -c \""
        "set -o pipefail; "
        "dump_xsettings | "
          "grep -E '^(Gtk/FontName |Xft/)' | "
          "sed -e 's/  */|/' | "
          "awk -F '|' '{printf \\\"" NAME_FORMAT "%s\\n\\\", \\$1, \\$2}'"
      "\"");
  if (WEXITSTATUS(retval) != 0) {
    printf("Install dump_xsettings from https://code.google.com/p/xsettingsd/\n"
           "to print this information.\n");
  }
  printf("\n");
}

int main(int argc, char** argv) {
  int opt;
  int bold = 0, italic = 0;
  const char* user_font_desc = NULL;
  while ((opt = getopt(argc, argv, "bf:hi")) != -1) {
    switch (opt) {
      case 'b':
        bold = 1;
        break;
      case 'f':
        user_font_desc = optarg;
        break;
      case 'i':
        italic = 1;
        break;
      default:
        fprintf(stderr,
                "Usage: %s [options]\n"
                "\n"
                "Options:\n"
                "  -b       Request bold font from Fontconfig\n"
                "  -f DESC  Specify Pango font description for Fontconfig\n"
                "  -i       Request italic font from Fontconfig\n",
                argv[0]);
        return 1;
    }
  }

  time_t now = time(NULL);
  printf("Running at %s\n", ctime(&now));

  gtk_init(&argc, &argv);
  PrintGtkSettings();
  PrintGtkStyles();
  PrintGnomeSettings();
  PrintXDisplayInfo();
  PrintXResources();
  PrintXSettings();
  PrintFontconfigSettings(user_font_desc, bold, italic);
  return 0;
}
