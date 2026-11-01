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
env.AddCustomTarget(
    name="ui-icons",
    dependencies=None,
    actions=['"$PYTHONEXE" "$PROJECT_DIR/tools/icons/generate_icons.py" --ui'],
    title="Regenerate Phosphor UI icons",
    description="Generate the selected build-time Phosphor weight, including true Duotone layers",
)
env.AddCustomTarget(
    name="ui-icons-force",
    dependencies=None,
    actions=['"$PYTHONEXE" "$PROJECT_DIR/tools/icons/generate_icons.py" --ui --force'],
    title="Regenerate Phosphor UI icons (re-download)",
    description="Re-download pinned Phosphor SVGs and regenerate the selected weight",
)
