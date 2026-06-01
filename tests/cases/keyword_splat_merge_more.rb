def capture(a, b: 1, **kw)
  [a, b, kw]
end

p capture(10, b: 2, c: 3)
p capture(10, **{b: 4, d: 5})
