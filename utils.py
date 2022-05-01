from collections import OrderedDict

# find first bit set in a word
def ffs(val):
    res = (val & -val).bit_length() - 1
    return res

# find last bit set in a word
def fls64(v):
    if v == 0:
        return 0
    result = 0

    if 0xFFFFFFFF00000000 & v:
        v >>= (1 << 5)
        result |= (1 << 5)
    if 0x00000000FFFF0000 & v:
        v >>= (1 << 4)
        result |= (1 << 4)
    if 0x000000000000FF00 & v:
        v >>= (1 << 3)
        result |= (1 << 3)
    if 0x00000000000000F0 & v:
        v >>= (1 << 2)
        result |= (1 << 2)
    if 0x000000000000000C & v:
        v >>= (1 << 1)
        result |= (1 << 1)
    if 0x0000000000000002 & v:
        result |= (1 << 0)
    return result

def rounddown_pow_of_two(n):
        return 1 << (fls64(n) - 1)

def round_up(x, y):
    mask = y - 1
    return ((x - 1) | mask) + 1

def round_down(x, y):
    mask = y - 1
    return x & ~mask

def is_power_of_2(n):
    return ((n & (n - 1)) == 0) and n != 0

def clamp(val, lo, hi):
    return min(max(val, lo), hi)

class LRUCache:
    # initialising capacity
    def __init__(self, capacity: int):
        self.cache = OrderedDict()
        self.capacity = capacity
 
    # we return the value of the key
    # that is queried in O(1) and return -1 if we
    # don't find the key in out dict / cache.
    # And also move the key to the end
    # to show that it was recently used.
    def get(self, key: int) -> int:
        if key not in self.cache:
            return -1
        else:
            self.cache.move_to_end(key)
            return self.cache[key]
 
    # first, we add / update the key by conventional methods.
    # And also move the key to the end to show that it was recently used.
    # But here we will also check whether the length of our
    # ordered dictionary has exceeded our capacity,
    # If so we remove the first key (least recently used)
    def put(self, key: int, value: int) -> None:
        self.cache[key] = value
        self.cache.move_to_end(key)
        if len(self.cache) > self.capacity:
            self.cache.popitem(last = False)
