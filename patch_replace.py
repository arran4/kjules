import sys

with open("src/mainwindow.cpp", "r") as f:
    content = f.read()

content = content.replace('.toString() == QStringLiteral("COMPLETED")) {', '.toString() == QStringLiteral("DONE")) {')

# Find the three duplicate action connects
def replace_action(content, action_name, target_state, state_name):
    # This is a bit fragile but since I know exactly what it looks like, I can replace it.
    start_str = f"  connect(\n      {action_name}, &QAction::triggered, this,\n      [this]() {{"
    end_str = "      });\n"

    start_idx = content.find(start_str)
    if start_idx == -1:
        print(f"Could not find {action_name}")
        return content

    end_idx = content.find(end_str, start_idx)
    if end_idx == -1:
        print(f"Could not find end of {action_name}")
        return content

    replacement = f"""  connect(
      {action_name}, &QAction::triggered, this,
      [this]() {{
        duplicateFollowingItemsToQueue(QStringLiteral("{target_state}"), i18n("{state_name}"));"""

    return content[:start_idx] + replacement + content[end_idx:]

content = replace_action(content, "m_duplicateFailedToQueueAndArchiveAction", "ERROR", "Failed")
content = replace_action(content, "m_duplicatePausedToQueueAndArchiveAction", "PAUSED", "Paused")
content = replace_action(content, "m_duplicateCanceledToQueueAndArchiveAction", "CANCELED", "Canceled")

with open("src/mainwindow.cpp", "w") as f:
    f.write(content)
