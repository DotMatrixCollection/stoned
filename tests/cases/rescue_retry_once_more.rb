count = 0

begin
  count += 1
  raise "again" if count < 2
rescue
  retry if count < 2
end

p count
