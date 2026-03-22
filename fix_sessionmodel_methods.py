import re

with open('src/mainwindow.cpp', 'r') as f:
    cpp = f.read()

# We need a getSessionById helper locally, or we can just iterate. Or better, we can add getSessionById and removeSessionById to SessionModel.
# But modifying SessionModel is easier. Let's patch SessionModel.
