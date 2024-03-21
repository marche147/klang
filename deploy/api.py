#!/usr/bin/env python3

import json, requests
from hashlib import md5

TT_URL = "https://www.aliyunctf.com/api/team/info"
TT_TOKEN = "sKqcquVEz8aNGGyHvQ9wD"

def check_teamtoken(token):
  r = requests.get(TT_URL, headers={'token':TT_TOKEN}, timeout=8)
  j = json.loads(r.text)
  assert j['msg'] == 'success'
  try:
    for team in j['data']:
      if team['hash'] == md5(token.encode()).hexdigest():
        return True
  except Exception as e :
    print(e)
    return False
  return False

__all__ = ["check_teamtoken"]