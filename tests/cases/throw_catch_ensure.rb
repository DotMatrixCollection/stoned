log = []
result = catch(:bail) do
  [1, 2, 3].each do |n|
    begin
      log << "body:#{n}"
      throw :bail, "threw at #{n}" if n == 2
    ensure
      log << "ens:#{n}"
    end
  end
end
p log
puts result
