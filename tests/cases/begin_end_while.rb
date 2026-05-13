count = 0

begin
  count += 1
end while false

begin
  count += 10
end until true

p count

def marker
  seen = []
  begin
    seen << :tick
  end while false
  seen
end

p marker
