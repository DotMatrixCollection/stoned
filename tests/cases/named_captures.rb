m = "2024-05-16".match(/(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/)
puts m[:year]
puts m[:month]
puts m["day"]
puts m[1]
