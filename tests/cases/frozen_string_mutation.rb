s = "hello".freeze
p s.frozen?

begin
  s.replace("other")
rescue FrozenError => e
  p "replace raised FrozenError"
rescue => e
  p "replace raised #{e.class}: #{e.message}"
end

begin
  s.upcase!
rescue FrozenError => e
  p "upcase! raised FrozenError"
rescue => e
  p "upcase! raised #{e.class}: #{e.message}"
end

begin
  s[0] = "H"
rescue FrozenError => e
  p "setindex raised FrozenError"
rescue => e
  p "setindex raised #{e.class}: #{e.message}"
end

p s
