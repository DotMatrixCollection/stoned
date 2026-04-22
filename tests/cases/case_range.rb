score = 85
grade = case score
when 90..100 then "A"
when 80..89  then "B"
when 70..79  then "C"
when 60..69  then "D"
else "F"
end
puts grade

n = 5
case n
when 1..3
  puts "low"
when 4..6
  puts "mid"
when 7..9
  puts "high"
end

# exclusive range boundary
case 10
when 1...10
  puts "inside exclusive"
when 10..20
  puts "at boundary inclusive"
end
