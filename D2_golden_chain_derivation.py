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
yt,off=build_table(THRESH,STREN,WIDE,PEAK)
ct,_=build_table(THRESH,STREN,WIDE,PEAK)
def tv(t,sd):
    i=sd+off; return t[i] if 0<=i<len(t) else 0
def blend(cur,prev):
    w=tv(yt,0)*tv(ct,cur-prev)   # luma diff 0 -> yr=254
    return (w*prev+(shift-w)*cur+shift1)>>shift2
def chain4(s):
    o0=s[0]; o1=(blend(s[1][0],o0[0]),blend(s[1][1],o0[1]))
    o2=(blend(s[2][0],o1[0]),blend(s[2][1],o1[1]))
    o3=(blend(s[3][0],o2[0]),blend(s[3][1],o2[1]))
    return o0,o1,o2,o3

# Search for a source set maximizing the MIN margin between the correct target and the two key
# recovery-bug discriminators (skip-both = prev anchor; skip-hole2 = prev output[1]), while keeping
# all values mid-range and distinct from prior chains.
best=None
import itertools
vals=list(range(40,220,8))
# fix luma 128; search U/V of 4 sources coarsely with structure: alternate high/low to create motion
cands=[]
for s1u in (200,208,216):
 for s2u in (160,168,176):
  for s3u in (104,112,120,128):
   for s0u in (56,64,72):
    s=[(s0u,184),(s1u,40),(s2u,72),(s3u,144)]
    o0,o1,o2,o3=chain4(s)
    skip2=(blend(s[3][0],o1[0]),blend(s[3][1],o1[1]))   # bug: target from hole-1
    skipb=(blend(s[3][0],o0[0]),blend(s[3][1],o0[1]))   # bug: target from anchor
    wrong=(blend(s[3][0],s[2][0]),blend(s[3][1],s[2][1]))
    # margins (abs diff per channel) for the closest/most-dangerous bug, skip-hole2
    m_skip2=min(abs(o3[0]-skip2[0]),abs(o3[1]-skip2[1]))
    m_all=min(abs(o3[0]-skip2[0]),abs(o3[1]-skip2[1]),
              abs(o3[0]-skipb[0]),abs(o3[1]-skipb[1]),
              abs(o3[0]-wrong[0]),abs(o3[1]-wrong[1]),
              abs(o3[0]-s[3][0]),abs(o3[1]-s[3][1]))
    # require holes distinct from each other too
    holes_distinct = (o1[0]!=o2[0] and o1[1]!=o2[1])
    if holes_distinct and m_all>=1:
        cands.append((m_skip2,m_all,s,o0,o1,o2,o3,skip2,skipb,wrong))
cands.sort(reverse=True)
m_skip2,m_all,s,o0,o1,o2,o3,skip2,skipb,wrong=cands[0]
print(f"BEST source set (max-margin on the skip-hole2 bug): skip2_margin={m_skip2} all_margin={m_all}")
print(f"source: 0=128/{s[0][0]}/{s[0][1]}  1=128/{s[1][0]}/{s[1][1]}  2=128/{s[2][0]}/{s[2][1]}  3=128/{s[3][0]}/{s[3][1]}")
print(f"anchor output[0]=128/{o0[0]}/{o0[1]}")
print(f"hole-1 output[1]=128/{o1[0]}/{o1[1]}")
print(f"hole-2 output[2]=128/{o2[0]}/{o2[1]}")
print(f"target output[3]=128/{o3[0]}/{o3[1]}   <-- D.2 GOLDEN")
print(f"discriminators: passthrough=128/{s[3][0]}/{s[3][1]}  wrong-pred=128/{wrong[0]}/{wrong[1]}  "
      f"skip-both=128/{skipb[0]}/{skipb[1]}  skip-hole2=128/{skip2[0]}/{skip2[1]}")
print(f"margins: skip-hole2 dU={abs(o3[0]-skip2[0])} dV={abs(o3[1]-skip2[1])}")

print("\n==================== D.2 LOCKED CHAIN + PROOF STRATEGY ====================")
s=[(72,184),(208,40),(176,72),(128,144)]
o0,o1,o2,o3=chain4(s)
print("LOCKED D.2 golden chain (8-bit YUV420, threshold=255 strength=255 narrow, luma const 128):")
print(f"  source[0]=128/72/184  source[1]=128/208/40  source[2]=128/176/72  source[3]=128/128/144")
print(f"  anchor output[0] = 128/{o0[0]}/{o0[1]}   (fresh-start)")
print(f"  hole-1 output[1] = 128/{o1[0]}/{o1[1]}   (filled from anchor)")
print(f"  hole-2 output[2] = 128/{o2[0]}/{o2[1]}   (filled from hole-1)")
print(f"  target output[3] = 128/{o3[0]}/{o3[1]}   (computed from hole-2)  D.2 GOLDEN")
print()
print("PROOF STRATEGY (robust, not target-margin-dependent):")
print("  1. KDT mechanism (load-bearing): branch=RECOVER hole_count=2 holes=[1,2]")
print("     source_requests=[1,2,3] hole=1 outcome=computed hole=2 outcome=computed pin_balance=0")
print("     -> proves the TWO-hole pin-list discharge (the D.2 new load-bearing thing).")
print("  2. Hole bytes via cache-hit follow-up: re-request frames 1,2 -> CACHE-HIT returns")
print(f"     output[1]=148/96 and output[2]=149/95 (well-separated) -> proves holes filled correctly+in-order.")
print(f"  3. Target byte: output[3]=148/100 (corroborating; note skip-hole2 bug = 147/101, only 1 LSB off,")
print("     so the target alone is NOT the primary proof -- the hole-byte check + KDT carry it).")
