p "hello\nworld\nfoo".lines.map(&:chomp)
p "hello\nworld\nfoo".lines.count
p "hello\nworld\nfoo".each_line.to_a.last
p "  hello  \n  world  \n".lines.map(&:strip)
