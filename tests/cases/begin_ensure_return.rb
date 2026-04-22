def f
  begin
    return 3
  ensure
    puts "ensure"
  end
end

puts f
