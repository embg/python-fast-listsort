from fastlist import FastList
import random; random.seed(42)
from time import time


print("*** 10 ints ***")
L = [random.randrange(int(10)) for _ in range(int(10))]

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)

print("*** 10 strings ***")
L = list(map(str, L))

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)

print("*** 1e3 ints ***")
L = [random.randrange(int(1e3)) for _ in range(int(1e3))]

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)

print("*** 1e3 strings ***")
L = list(map(str, L))

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)

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

print("*** 1e7 ints + 1 float (to disable the optimization while keeping the precheck)***")
L = [random.randrange(int(1e7)) for _ in range(int(1e7))] + [4.2]

F = FastList(L[:])
start = time(); F.fastsort(); print("F.fastsort():", time()-start)

F = FastList(L[:])
start = time(); F.sort(); print("F.sort():", time()-start)
