"""Compile the production directory scanners with deterministic OS/allocation fixtures."""
from pathlib import Path
import subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import compile as compiler
import toolchain
from run_regressions import function
out=ROOT/'build/regressions';out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'code/mcm2/dirlist.c').read_text()
names=['static dir_entry* entries_new(', 'BOOL __fastcall dir_list_wanted(', 'BOOL __fastcall dir_list_scan(',
       'dir_list* __fastcall dir_list_construct(', 'void __fastcall dir_list_set_folder(',
       'void __fastcall dir_list_set_pattern(', 'void __fastcall dir_list_sort(',
       'void __fastcall dir_list_destruct(', 'BOOL __fastcall merged_list_scan(']
(out/'directory_under_test.inc').write_text('\n'.join(function(s,n) for n in names))
env=compiler.msvc_environment();native=toolchain.native
subprocess.run(toolchain.command('cl','x86',env)+['/nologo','/std:c11','/O2','/W4','/WX','/wd4100','/D_CRT_SECURE_NO_WARNINGS',
 '/I'+native(ROOT/'code/mcm2'),'/I'+native(out),native(ROOT/'tests/directory_regressions.c'),
 '/Fe'+native(out/'directory.exe'),'/Fo'+native(out)+'\\','/link','kernel32.lib'],cwd=ROOT,env=env,check=True)
subprocess.run(toolchain.run_exe(str(out/'directory.exe')),check=True)
