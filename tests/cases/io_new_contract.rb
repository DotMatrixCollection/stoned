io = IO.new(1)
puts io.class
io.close

io = IO.new(1, nil)
puts io.class
io.close

tests = [
  [:fd_nil, nil, "w"],
  [:fd_str, "1", "w"],
  [:fd_true, true, "w"],
  [:mode_false, 1, false],
  [:mode_array, 1, []],
  [:mode_bad, 1, "q"],
  [:fd_neg, -1, "w"],
]

tests.each do |name, fd, mode|
  begin
    IO.new(fd, mode)
  rescue => e
    puts name
    puts e.class
    puts e.message
  end
end
