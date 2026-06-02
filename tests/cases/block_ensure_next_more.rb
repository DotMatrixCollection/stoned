seen = []

[1, 2, 3].each do |n|
  begin
    next if n == 2
    seen << "body:#{n}"
  ensure
    seen << "ensure:#{n}"
  end
end

p seen
