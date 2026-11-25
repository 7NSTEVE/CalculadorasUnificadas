import os
import sys
import time

_tools_dir = os.path.dirname(os.path.abspath(__file__))
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
from calc_visual_auditor import CalcVisualAuditor

auditor = CalcVisualAuditor(target="unified", outdir="verification/diag_keys")
auditor.start()
try:
    auditor.press_sequence(["1"])
    time.sleep(2.5)
    auditor.press_sequence(["AC"], default_wait=0.3)
    
    # Test LN
    print("Testing LN...")
    auditor.press_sequence(["LN"])
    time.sleep(1.0)
    r_ln = auditor.capture_and_audit("diag_ln")
    print("OCR LN:", r_ln["ocr_text"].replace("\n", " "))

    # Clear
    auditor.press_sequence(["AC"], default_wait=0.3)

    # Test SIN
    print("Testing SIN...")
    auditor.press_sequence(["SIN"])
    time.sleep(1.0)
    r_sin = auditor.capture_and_audit("diag_sin")
    print("OCR SIN:", r_sin["ocr_text"].replace("\n", " "))

    # Clear
    auditor.press_sequence(["AC"], default_wait=0.3)

    # Test COS
    print("Testing COS...")
    auditor.press_sequence(["COS"])
    time.sleep(1.0)
    r_cos = auditor.capture_and_audit("diag_cos")
    print("OCR COS:", r_cos["ocr_text"].replace("\n", " "))

    # Clear
    auditor.press_sequence(["AC"], default_wait=0.3)

    # Test X (X_THETA)
    print("Testing X...")
    auditor.press_sequence(["X"])
    time.sleep(1.0)
    r_x = auditor.capture_and_audit("diag_x")
    print("OCR X:", r_x["ocr_text"].replace("\n", " "))

finally:
    auditor.stop()
