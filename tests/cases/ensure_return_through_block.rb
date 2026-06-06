def early_return
  [1, 2, 3].each do |n|
    begin
      return "got #{n}" if n == 2
    ensure
      puts "ensure:#{n}"
    end
  end
  "done"
end
puts early_return
