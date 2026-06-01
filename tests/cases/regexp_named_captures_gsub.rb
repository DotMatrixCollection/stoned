re = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/
m = re.match("2024-03-15")
p m[:year]
p m[:month]
p m[:day]
p m.named_captures

result = "John Smith".gsub(/(?<first>\w+) (?<last>\w+)/, '\k<last>, \k<first>')
p result
