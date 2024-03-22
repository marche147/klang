#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import hashlib, string, struct, sys

def solve_pow(chal, n):
  r = 0
  while True:
    s = chal + struct.pack("<Q", r)
    h = int(hashlib.sha256(s).hexdigest(), 16)
    if h % (2 ** n) == 0:
      break
    r += 1
  return r

if __name__ == '__main__':
  if len(sys.argv) != 3:
    print('Usage: python pow_solver.py <chal> <difficulty>')
    sys.exit(1)
  result = solve_pow(sys.argv[1].encode(), int(sys.argv[2]))
  print(result)
