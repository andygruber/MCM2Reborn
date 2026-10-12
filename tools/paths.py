# -*- coding: utf-8 -*-
"""Where the tools find the retail executable: MCM2_EXE, or the copy in the Ghidra workspace."""
import os

RETAIL = os.environ.get("MCM2_EXE", r"C:\mcm2ghidra\bin\Mcm2.exe")
