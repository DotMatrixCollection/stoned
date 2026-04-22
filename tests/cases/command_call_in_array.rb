class A
  def add(x)
    x + 1
  end
end

puts [A.new.add 2, A.new.add 3].join(",")
