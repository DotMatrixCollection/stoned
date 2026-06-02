require 'weakref'

obj = "hello"
ref = WeakRef.new(obj)
p ref.__getobj__
p ref.weakref_alive?
p ref.__getobj__.class

arr = [1, 2, 3]
wref = WeakRef.new(arr)
p wref.__getobj__.length
p wref.weakref_alive?

# WeakRef responds to methods of the wrapped object
str = "world"
wstr = WeakRef.new(str)
p wstr.upcase
p wstr.length

# WeakRef identity
p ref.is_a?(WeakRef)
p ref.class
