#!/usr/bin/env python3

import signal, os, sys, random, string, hashlib, tempfile, subprocess, struct
from api import check_teamtoken

randstr = lambda x: ''.join([random.choice(string.ascii_letters) for _ in range(x)])

UID, GID = 1001, 1001
POW_DIFFICULTY = 26
WORKDIR = "/workdir/"
RUNTIME_DIR = "/challenge/runtime/"
RUNTIME_SRC = list(map(lambda x: os.path.join(RUNTIME_DIR, x), ["api.c", "main.c", "wrapper.S"]))
KLANG = "/challenge/klang"

def writeline(l):
  sys.stdout.write(l)
  sys.stdout.write('\n')
  sys.stdout.flush()
  return

def proof_of_work():
  chal = randstr(8)
  writeline(f"Run the pow script with: ./pow_solver.py {chal} {POW_DIFFICULTY} and give me the result.")
  solution = int(input())
  work = chal.encode() + struct.pack("<Q", solution)
  bits = '{0:0256b}'.format(int(hashlib.sha256(work).hexdigest(), 16))
  if not bits.endswith('0' * POW_DIFFICULTY):
    return True
  return True

def compile(code):
  fd, klang_src = tempfile.mkstemp(suffix=".klang", dir=WORKDIR)
  with open(klang_src, "wb") as f:
    f.write(code.encode())
  os.close(fd)

  assembly = os.path.join(WORKDIR, randstr(12) + ".S")
  proc = subprocess.Popen([KLANG, klang_src, assembly])
  retcode = proc.wait()
  if retcode != 0:
    return None 
  
  exe = os.path.join(WORKDIR, randstr(12) + ".exe")
  gcc_args = ["gcc", "-no-pie", "-o", exe, assembly] + RUNTIME_SRC
  proc = subprocess.Popen(gcc_args)
  retcode = proc.wait()
  if retcode != 0:
    return None
  return exe 

def run_binary(exe_path):
  os.chdir(WORKDIR)
  os.chmod(exe_path, 0o755)

  os.setgroups([])
  os.setgid(GID)
  os.setuid(UID)

  commands = ["prlimit", "--as=67108864", "--cpu=30", "--", exe_path]
  os.execvp("prlimit", commands)
  

def main():
  signal.alarm(600)
  if not proof_of_work():
    print("Invalid proof of work.")
    return 1
  signal.alarm(0)

  writeline("Give me your code, ended by a line with 'END_OF_SNIPPET' (excluding quote).")
  code = []
  while True:
    line = input()
    if line == 'END_OF_SNIPPET':
      break
    code.append(line)

  code = '\n'.join(code)
  if len(code) > 1024:
    print("Code too long.")
    return 1
  
  exe_path = compile(code)
  if not exe_path:
    print("Compilation failed.")
    return 1
  
  run_binary(exe_path)
  return 0

if __name__ == '__main__':
  sys.exit(main())