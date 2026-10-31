"""Compile the production directory scanners with deterministic OS/allocation fixtures."""
from pathlib import Path
import os,subprocess,sys,shutil
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import compile as compiler
from run_regressions import function
out=ROOT/'build/regressions';out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'code/mcm2/dirlist.c').read_text()
names=['static dir_entry* entries_new(', 'BOOL __fastcall dir_list_wanted(', 'BOOL __fastcall dir_list_scan(',
       'dir_list* __fastcall dir_list_construct(', 'void __fastcall dir_list_set_folder(',
       'void __fastcall dir_list_set_pattern(', 'void __fastcall dir_list_sort(',
       'void __fastcall dir_list_destruct(', 'BOOL __fastcall merged_list_scan(']
(out/'directory_under_test.inc').write_text('\n'.join(function(s,n) for n in names))
env=compiler.msvc_environment();cl=shutil.which('cl',path=env.get('Path') or env.get('PATH'))
subprocess.run([cl,'/nologo','/std:c11','/O2','/W4','/WX','/wd4100','/D_CRT_SECURE_NO_WARNINGS',
 '/I'+str(ROOT/'code/mcm2'),'/I'+str(out),str(ROOT/'tests/directory_regressions.c'),
 '/Fe'+str(out/'directory.exe'),'/Fo'+str(out)+os.sep,'/link','kernel32.lib'],env=env,check=True)
subprocess.run([str(out/'directory.exe')],check=True)
