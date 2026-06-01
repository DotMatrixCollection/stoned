arr = [[1,2], [3,4]].freeze
p arr.frozen?
p arr[0].frozen?
begin
  arr << [5,6]
rescue => e
  p e.class
end
arr[0] << 99
p arr[0]

n = 0b1010
p n[0]
p n[1]
p n[2]
p n[3]
p n[4]
p 255[7]
