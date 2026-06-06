results = []
[1, 2, 3].each do |n|
  begin
    break if n == 2
    results << "body:#{n}"
  ensure
    results << "ensure:#{n}"
  end
end
p results
