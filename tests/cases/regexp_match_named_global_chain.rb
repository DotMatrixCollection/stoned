"2026-06-05" =~ /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/
p $~[:year]
p $~[:month]
p $~[:day]
p $~.named_captures.values.map(&:to_i)
