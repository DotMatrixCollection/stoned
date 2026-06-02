text = "ruby 30 python 31"
p text.scan(/(\w+) (\d+)/)
p text.scan(/(?<lang>\w+) (?<ver>\d+)/)
p /(?<year>\d{4})-(?<month>\d{2})/.named_captures
m = "2024-06".match(/(?<year>\d{4})-(?<month>\d{2})/)
p m.named_captures
