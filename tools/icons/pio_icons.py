Import("env")

# `platformio run -t icons` regenerates the full-color images in tools/icons/icons.json.
env.AddCustomTarget(
    name="icons",
    dependencies=None,
    actions=['"$PYTHONEXE" "$PROJECT_DIR/tools/icons/generate_icons.py"'],
    title="Regenerate icons",
    description="Download missing source SVGs and regenerate LVGL image .c files",
)
env.AddCustomTarget(
    name="icons-force",
    dependencies=None,
    actions=['"$PYTHONEXE" "$PROJECT_DIR/tools/icons/generate_icons.py" --force'],
    title="Regenerate icons (re-download)",
    description="Re-download source SVGs, reinstall the renderer and regenerate all icons",
)
