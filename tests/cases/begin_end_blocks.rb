BEGIN { puts "begin1" }
END { puts "end1" }
puts "main"
END { puts "end2" }
puts "more"
BEGIN { puts "begin2" }
