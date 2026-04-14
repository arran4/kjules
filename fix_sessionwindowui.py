with open("src/sessionwindowui.rc", "r") as f:
    content = f.read()

content = content.replace("    <Action name=\"duplicate_session\"/>\n    <Separator/>\n    <Action name=\"close_window\"/>",
                          "    <Action name=\"duplicate_session\"/>\n    <Separator/>\n    <Action name=\"auto_refresh_combo\"/>\n    <Separator/>\n    <Action name=\"close_window\"/>")

with open("src/sessionwindowui.rc", "w") as f:
    f.write(content)
