Import("env")

# `platformio run -t fonts` regenerates every font in tools/fonts/fonts.json.
# Limit it with HG_FONTS, e.g. PowerShell: $env:HG_FONTS = "icons_36,inter_96".
env.AddCustomTarget(
    name="fonts",
    dependencies=None,
    actions=['"$PYTHONEXE" "$PROJECT_DIR/tools/fonts/generate_fonts.py"'],
    title="Regenerate fonts",
    description="Download missing source fonts and regenerate LVGL font .c files",
)
env.AddCustomTarget(
    name="fonts-force",
    dependencies=None,
    actions=['"$PYTHONEXE" "$PROJECT_DIR/tools/fonts/generate_fonts.py" --force'],
    title="Regenerate fonts (re-download)",
    description="Re-download source fonts, reinstall converters and regenerate all fonts",
)
