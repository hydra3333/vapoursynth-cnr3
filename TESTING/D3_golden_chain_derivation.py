import math
PEAK=255; BITS=8; THRESH=255; STREN=255; WIDE=False
shift2=BITS<<1; shift=1<<shift2; shift1=shift>>1
def clamp(v,lo,hi): return lo if v<lo else hi if v>hi else v
def build_table(threshold,strength,wide,peak):
    size=2*peak+1; off=peak; t=[0]*size
    threshold=clamp(threshold,0,peak); strength=clamp(strength,0,peak)
    if threshold==0: t[off]=strength; return t,off
    for sd in range(-threshold,threshold+1):
        idx=sd+off; ad=abs(sd)
        angle=(ad*ad*math.pi/(threshold*threshold)) if wide else (ad*math.pi/threshold)
        half=float(strength//2)
        t[idx]=clamp(int(half*(1.0+math.cos(angle))),0,peak)
    return t,off
yt,off=build_table(THRESH,STREN,WIDE,PEAK); ct,_=build_table(THRESH,STREN,WIDE,PEAK)
def tv(t,sd):
    i=sd+off; return t[i] if 0<=i<len(t) else 0
def blend(cur,prev):  # luma diff 0 -> yr=254
    w=tv(yt,0)*tv(ct,cur-prev)
    return (w*prev+(shift-w)*cur+shift1)>>shift2

print("==================== D.3 FLOOR-FRESH-START RECOVERY CHAIN ====================")
print("Scenario: request output[N]; NO present output anywhere below N in the window")
print("[max(0,N-B), N-1].  Recovery computes the FLOOR frame as a FRESH-START (output=copyFrame")
print("(source[floor]), chroma UNCHANGED, no predecessor), then fills holes ASCENDING to N.")
print()
print("HARNESS CONSTRUCTION NOTE: with B=50, 'no anchor in window' for a small N means N must be")
print("<= B so the window floor is 0 -- but frame 0, if computed, would BE an anchor. To get a")
print("genuine floor-fresh-start in a small harness we need the window's floor frame itself to be")
print("the start AND nothing below N present. Simplest: request output[N] with N <= B (so floor=0)")
print("on a clip where NOTHING has been computed yet -> the descending search finds nothing in")
print("[0, N-1], floor=0, fresh-start at 0, fill 1..N. (This is just a cold first-request for N>0.)")
print()

# Cold first-request for output[2] with N=2, B=50 -> window [0,1], nothing present -> floor=0 fresh-start.
# floor output[0] = source[0] (fresh-start, chroma unchanged)
# hole  output[1] = blend(source[1], output[0])
# target output[2] = blend(source[2], output[1])
# This is actually the SAME math as a fresh chain from 0 -- the distinguishing feature of D.3 is that
# it's reached via the RECOVERY branch's FLOOR FALLBACK, not via sequential predecessor-present.
# Use D.3-distinct source values (different from D.1 80/176.., D.2 72/184..).
s=[(64,168),(200,56),(160,88)]   # source[0],[1],[2]
o0=s[0]
o1=(blend(s[1][0],o0[0]), blend(s[1][1],o0[1]))
o2=(blend(s[2][0],o1[0]), blend(s[2][1],o1[1]))
print(f"source[0]=128/{s[0][0]}/{s[0][1]}  source[1]=128/{s[1][0]}/{s[1][1]}  source[2]=128/{s[2][0]}/{s[2][1]}")
print(f"floor  output[0] = 128/{o0[0]}/{o0[1]}   (FRESH-START: copyFrame(source[0]), chroma unchanged)")
print(f"hole   output[1] = 128/{o1[0]}/{o1[1]}   (filled from floor)")
print(f"target output[2] = 128/{o2[0]}/{o2[1]}   (computed from hole)   <-- D.3 GOLDEN")
print()
# discriminators
print(f"DISCRIMINATORS for target output[2]=128/{o2[0]}/{o2[1]}:")
print(f"  passthrough(source[2]) = 128/{s[2][0]}/{s[2][1]}  distinct U:{o2[0]!=s[2][0]} V:{o2[1]!=s[2][1]}")
wp=(blend(s[2][0],s[1][0]),blend(s[2][1],s[1][1]))
print(f"  wrong-pred(source[1])  = 128/{wp[0]}/{wp[1]}  distinct U:{o2[0]!=wp[0]} V:{o2[1]!=wp[1]}")
sk=(blend(s[2][0],o0[0]),blend(s[2][1],o0[1]))
print(f"  skip-hole(prev=floor)  = 128/{sk[0]}/{sk[1]}  distinct U:{o2[0]!=sk[0]} V:{o2[1]!=sk[1]}")
print(f"  D.1 147/109 distinct: U:{o2[0]!=147} V:{o2[1]!=109} ; D.2 148/100 distinct: U:{o2[0]!=148} V:{o2[1]!=100}")

print("\n==================== D.3 N=3 FLOOR-FRESH-START CHAIN (coder-preferred) ====================")
print("Cold request output[3]: floor=0 (fresh-start), holes=[1,2], target=3. floor + 2 holes + target.")
# distinct D.3 source values (avoid D.1 80/176.., D.2 72/184.., and the earlier D.3-N2 64/168 set)
s=[(56,176),(200,48),(168,80),(120,152)]
o0=s[0]                                            # floor fresh-start: chroma unchanged
o1=(blend(s[1][0],o0[0]), blend(s[1][1],o0[1]))    # hole 1 from floor
o2=(blend(s[2][0],o1[0]), blend(s[2][1],o1[1]))    # hole 2 from hole 1
o3=(blend(s[3][0],o2[0]), blend(s[3][1],o2[1]))    # target from hole 2
print(f"source[0]=128/{s[0][0]}/{s[0][1]} source[1]=128/{s[1][0]}/{s[1][1]} source[2]=128/{s[2][0]}/{s[2][1]} source[3]=128/{s[3][0]}/{s[3][1]}")
print(f"floor  output[0] = 128/{o0[0]}/{o0[1]}   (FRESH-START: copyFrame(source[0]), chroma unchanged)")
print(f"hole-1 output[1] = 128/{o1[0]}/{o1[1]}   (filled from floor)")
print(f"hole-2 output[2] = 128/{o2[0]}/{o2[1]}   (filled from hole-1)")
print(f"target output[3] = 128/{o3[0]}/{o3[1]}   (computed from hole-2)   <-- D.3 GOLDEN")
print()
print("PROOF STRATEGY (same as D.2: KDT mechanism + hole-byte cache-hit primary; target corroborates):")
print(f"  KDT: branch=RECOVER recover_branch=floor-fresh-start floor=0 hole_count=2 holes=[1,2]")
print(f"       source_requests=[0,1,2,3] hole=1 outcome=computed hole=2 outcome=computed")
print(f"       pin_list_size=3 pin_balance=0  (floor pin + 2 hole pins = 3)")
print(f"  hole bytes via cache-hit: output[1]=128/{o1[0]}/{o1[1]}  output[2]=128/{o2[0]}/{o2[1]}")
print(f"  floor byte via cache-hit: output[0]=128/{o0[0]}/{o0[1]} (== source[0], proves fresh-start chroma-unchanged)")
print(f"  target: output[3]=128/{o3[0]}/{o3[1]}")
print()
# discriminators for target + the all-important floor-correctness check
print("DISCRIMINATORS:")
print(f"  passthrough(source[3]) = 128/{s[3][0]}/{s[3][1]}  distinct U:{o3[0]!=s[3][0]} V:{o3[1]!=s[3][1]}")
wp=(blend(s[3][0],s[2][0]),blend(s[3][1],s[2][1]))
print(f"  wrong-pred(source[2])  = 128/{wp[0]}/{wp[1]}  distinct U:{o3[0]!=wp[0]} V:{o3[1]!=wp[1]}")
# the D.3-specific bug: if floor were computed as a BLEND (wrong) instead of fresh-start copy.
# fresh-start floor MUST equal source[0]; if a bug blended floor against some default(e.g. 0), output[0] differs.
print(f"  floor MUST = source[0] = 128/{s[0][0]}/{s[0][1]} (fresh-start chroma unchanged); any blend at floor -> wrong chain")
print(f"  D.1 147/109 distinct U:{o3[0]!=147} V:{o3[1]!=109}; D.2 148/100 distinct U:{o3[0]!=148} V:{o3[1]!=100}")
# source set
print(f"\nSOURCE REQUEST SET (derived, NOT hardcoded): {{floor..N}} = {{0,1,2,3}} -> source_requests=[0,1,2,3]")
