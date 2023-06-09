import json
import os.path
import sys

import bs4
import requests

if os.path.exists('desktop-specific.json'):
    os.chdir('../')

req = requests.get(
    'https://raw.githubusercontent.com/AyuGram/AyuGram4A/rewrite/TMessagesProj/src/main/res/values/ayu.xml'
)

tree = bs4.BeautifulSoup(req.text, 'xml')

strings = {}
for string in tree.find_all('string'):
    t = string.attrs['name']
    strings[f'ayu_{t}'] = string.text

with open('./ayu-scripts/desktop-specific.json') as f:
    data = json.load(f)

    for k, v in data.items():
        strings[f'ayu_{k}'] = v

req = requests.get(
    'https://raw.githubusercontent.com/telegramdesktop/tdesktop/dev/Telegram/Resources/langs/lang.strings'
)

data = req.text

data += '''

// AyuGram keys generated

'''

for k, v in strings.items():
    escaped = v.replace('"', '\\"')
    data += f'"{k}" = "{escaped}";\n'

with open(os.path.realpath('./Telegram/Resources/langs/lang.strings'), 'w', encoding='utf-8') as f:
    f.write(data)

print('Done.')
