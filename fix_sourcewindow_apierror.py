import re

with open('src/sourcewindow.h', 'r') as f:
    content = f.read()

# I missed including apierror.h in sourcewindow.h!
if '#include "api/apierror.h"' not in content:
    content = re.sub(
        r'#include <KXmlGuiWindow>',
        '#include <KXmlGuiWindow>\n#include "api/apierror.h"',
        content
    )

with open('src/sourcewindow.h', 'w') as f:
    f.write(content)
