import re

with open('src/mainwindow.cpp', 'r') as f:
    cpp = f.read()

# Replace SessionWindow instantiations. We'll determine isManaged by checking if it's in m_sessionModel.
# Actually we can just do: bool isManaged = m_sessionModel->contains(id);
# wait, does SessionModel have contains()? Let's check sessionmodel.h
