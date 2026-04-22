import sys

# In src/mainwindow.cpp we should move `template <typename ActionFunc> void MainWindow::applyFavouriteAction(ActionFunc action) {`
# to the header because it's a template, or we need to remove the template and just use `std::function`.
# Using a template method in a .cpp file causes linking errors if not instantiated, although here it's only used within the same translation unit. Wait, the error is `Template function as signal or slot`.
# It's in the header file:
# Error: Template function as signal or slot
# Ah! We moved it OUT of Q_SLOTS in fix_slots.py, but it seems MOC still threw an error.
# Let's check where it is in mainwindow.h right now:
