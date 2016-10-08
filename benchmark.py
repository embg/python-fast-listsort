from fastlist import FastList
import random; random.seed(42)
from time import time

print("*** 1e7 ints ***")
L = [random.randrange(int(1e7)) for _ in range(int(1e7))]

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)

print("*** 1e7 strings ***")
L = list(map(str, L))

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)
