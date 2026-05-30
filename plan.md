1. Modify `FilterEditor` so the popup is exactly the width of the `m_lineEdit`.
2. Add a focus event filter on `m_lineEdit` so that when it gains focus, the popup becomes visible (if not explicitly dismissed or maybe even if it is? The user says "become visible upon focus and lost upon exiting OR toggled").
3. Implement `focusInEvent` and `focusOutEvent` handling for `m_lineEdit`. Wait, the user said "It should become visible upon focus and lost upon exiting". So we need to watch `m_lineEdit`'s focus.
4. Ensure the popup's width in `updatePopupPosition` uses `m_lineEdit->width()` and aligns with `m_lineEdit->mapToGlobal(...)`.
5. Since the focus could be lost to the popup itself (if the user clicks on the list or tree), we must not hide the popup if the new focus widget is a child of `m_popupFrame`.
6. Update `m_toggleButton` logic to work with this focus-based visibility.
